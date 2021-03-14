/**
 * 网络层底层
 * 
 * 工作线程与socket线程如何通信:
 * 1. 写数据：通过send_request这个api向发送管道写数据，数据额外包含类型type
 * 2. 读数据
 * 
 * 如何处理网络收发数据:
 * 当一个连接建立时, 会将fd加入到poller中.
 * 并且将该socket fd传递给poller事件集, 目的是当epoll事件触发时可以找到对应的socket对象而做对应的处理。
 */

#include "socket_server.h"
#include "poller.h"

#include "../utils/socket_helper.h"

#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cstdbool>
#include <cstring>
#include <iostream>
#include <mutex>
#include <atomic>
#include <cassert>
#include <cerrno>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

namespace skynet { namespace socket {

enum
{
    MIN_READ_BUFFER                 = 64,
    
    WARNING_SIZE                    = 1024 * 1024,

    SIZEOF_TCP_BUFFER               = offsetof(write_buffer, udp_address[0]),
    SIZEOF_UDP_BUFFER               = sizeof(write_buffer),
};

// 缓存优先级
enum priority_type
{
    HIGH                        = 0,                                        // 高
    LOW                         = 1,                                        // 低
};

socket_server::~socket_server()
{
    fini();
}

// 初始化
bool socket_server::init(uint64_t time/* = 0*/)
{
    //
    time_ = time;

    // 创建poll fd (epoll or kqueue)
    if (!event_poller_.init())
    {
        std::cerr << "socket-server: create event poll fd failed." << std::endl;
        return false;
    }

    //    
    if (!pipe_.init())
    {
        std::cerr << "socket-server: create pipe failed." << std::endl;
        return false;
    }

    // add read ctrl fd to event poll
    int pipe_read_fd = pipe_.read_fd();
    if (!event_poller_.add(pipe_read_fd, nullptr))
    {
        std::cerr << "socket-server: can't add server socket fd to event poll." << std::endl;
        
        pipe_.fini();
        return false;
    }

    //
    ::memset(&soi_, 0, sizeof(soi_));

    return true;
}

// 清理
void socket_server::fini()
{
    //
    socket_message dummy;
    for (auto& socket_ref : socket_slot_)
    {
        if (socket_ref.status != socket::status::ALLOCED)
        {
            socket_lock sl(socket_ref.dw_mutex);
            force_close(&socket_ref, sl, &dummy);
        }
    }

    // 清理pipe
    pipe_.fini();

    //
    event_poller_.fini();
}

void socket_server::start(uint64_t svc_handle, int socket_id)
{
    ctrl_cmd_package cmd;
    prepare_ctrl_cmd_request_start(cmd, svc_handle, socket_id);
    _send_ctrl_cmd(&cmd);
}

void socket_server::exit()
{
    ctrl_cmd_package cmd;
    _send_ctrl_cmd(&cmd);
}

void socket_server::close(uint64_t svc_handle, int socket_id)
{
    ctrl_cmd_package cmd;
    prepare_ctrl_cmd_request_close(cmd, svc_handle, socket_id);
    _send_ctrl_cmd(&cmd);
}

void socket_server::shutdown(uint64_t svc_handle, int socket_id)
{
    ctrl_cmd_package cmd;
    prepare_ctrl_cmd_request_shutdown(cmd, svc_handle, socket_id);
    _send_ctrl_cmd(&cmd);
}

void socket_server::update_time(uint64_t time)
{
    time_ = time;
}

int socket_server::poll_socket_event(socket_message* result, bool& is_more)
{
    for (;;)
    {
        // 检查控制命令数据
        if (need_check_ctrl_cmd_)
        {
            // pipe有数据可读
            if (pipe_.is_readable())
            {
                int type = _recv_ctrl_cmd(result);
                if (type == -1)
                    continue;

                // 需要清理 closed 事件
                if (type == socket_event::SOCKET_CLOSE || type == socket_event::SOCKET_ERROR)
                {
                    _clear_closed_event(result->socket_id);
                }

                return type;
            }

            // pipe无数据
            need_check_ctrl_cmd_ = false;
        }

        // 没有等待处理的事件 (事件已处理完)
        if (event_next_index_ == event_wait_n_)
        {
            //
            need_check_ctrl_cmd_ = true;
            is_more = false;

            // 获取需要处理的事件数目
            event_wait_n_ = event_poller_.wait(events_);

            // 重置读取下标
            event_next_index_ = 0;
            if (event_wait_n_ <= 0)
            {
                event_wait_n_ = 0;

                // wait interupte
                if (errno == EINTR) 
                    continue;

                return -1;
            }
        }
        
        // 从事件中获取对应的socket
        auto& event_ref = events_[event_next_index_++];
        auto socket_ptr = (socket*)event_ref.socket_ptr;
        if (socket_ptr == nullptr)
            continue;

        // 根据socket状态做相应的处理
        uint8_t status = socket_ptr->status;
        switch (status)
        {
        // socket正在连接
        case socket::status::CONNECTING:
            return report_connect(socket_ptr, result);
        //
        case socket::status::LISTEN:
            {
                int ok = report_accept(socket_ptr, result);
                if (ok > 0)
                    return socket_event::SOCKET_ACCEPT;
                if (ok < 0)
                    return socket_event::SOCKET_ERROR;
                
                // ok == 0, retry
                break;
            }
        case socket::status::FREE:
            std::cerr << "socket-server : invalid socket" << std::endl;
            break;
        default:
            //
            socket_lock sl(socket_ptr->dw_mutex);

            // 如果socket已连接且事件可读，通过forward_message_tcp接收数据
            if (event_ref.is_readable)
            {
                int type;
                if (socket_ptr->protocol == protocol_type::TCP)
                {
                    type = forward_message_tcp(socket_ptr, sl, result);
                }
                else
                {
                    type = forward_message_udp(socket_ptr, sl, result);

                    // 尝试再次读取
                    if (type == socket_event::SOCKET_UDP)
                    {
                        --event_next_index_;
                        return socket_event::SOCKET_UDP;
                    }
                }

                // Try to dispatch write message next step if write flag set.
                if (event_ref.is_writeable &&
                    type != socket_event::SOCKET_CLOSE && 
                    type != socket_event::SOCKET_ERROR)
                {
                    event_ref.is_readable = false;
                    --event_next_index_;
                }

                //
                if (type == -1)
                    break;
                
                return type;
            }

            // 如果socket已连接且事件可写，通过send_buffer发送数据。
            if (event_ref.is_writeable)
            {
                int type = send_write_buffer(socket_ptr, sl, result);
                
                // blocked, 稍后再发
                if (type == -1)
                    break;
                
                return type;
            }

            // close when error
            if (event_ref.is_error)
            {
                int error = 0;
                socklen_t len = sizeof(error);
                int code = ::getsockopt(socket_ptr->socket_fd, SOL_SOCKET, SO_ERROR, &error, &len);
                const char* err = nullptr;
                if (code < 0)
                    err = ::strerror(errno);
                else if (error != 0)
                    err = ::strerror(error);
                else
                    err = "Unknown error";
                
                force_close(socket_ptr, sl, result);
                result->data = const_cast<char*>(err);

                return socket_event::SOCKET_ERROR;
            }
            
            // eof
            if (event_ref.is_eof)
            {
                force_close(socket_ptr, sl, result);
                return socket_event::SOCKET_CLOSE;
            }

            break;
        }
    }
}

// 
int socket_server::listen(uint64_t svc_handle, const char* addr, int port, int backlog)
{
    // 调用unix系统接口bind，listen获取一个fd
    int listen_fd = socket_helper::listen(addr, port, backlog);
    if (listen_fd < 0)
        return -1;

    // 从ss的socket池中获取空闲的socket, 并返回id
    int socket_id = _alloc_socket_id();
    if (socket_id < 0)
    {
        ::close(listen_fd);
        return socket_id;
    }

    // 保存关联的服务地址，socket池的id，socket套接字fd
    ctrl_cmd_package cmd;
    prepare_ctrl_cmd_request_listen(cmd, svc_handle, socket_id, listen_fd);
    _send_ctrl_cmd(&cmd);
    
    return socket_id;
}

int socket_server::send(send_buffer* buf)
{
    int socket_id = buf->socket_id;
    auto& socket_ref = socket_slot_[calc_slot_index(socket_id)];
    
    // 检查socket状态
    if (socket_ref.socket_id != socket_id || socket_ref.status == socket::status::FREE)
    {
        free_send_buffer(buf);
        return -1;
    }

    // 是否可以立刻发送数据，当该socket的发送队列缓冲区为空，且立刻写的缓冲区也为空时，可直接发送。
    if (socket_ref.can_direct_write(socket_id))
    {
        // scope lock
        std::unique_lock<std::mutex> sl(socket_ref.dw_mutex);

        if (sl.try_lock())
        {
            // may be we can send directly, double check
            if (socket_ref.can_direct_write(socket_id))
            {
                // 直接发送
                send_object so;
                send_object_init(&so, buf);
                ssize_t n = 0;

                // tcp
                if (socket_ref.protocol == protocol_type::TCP)
                {
                    n = ::write(socket_ref.socket_fd, so.buffer, so.sz);
                }
                // udp
                else
                {
                    socket_addr sa;
                    int sa_sz = udp_address_to_socket_addr(socket_ref.protocol, socket_ref.p.udp_address, sa);
                    if (sa_sz == 0)
                    {
                        std::cerr << "socket-server : set udp (" << socket_id << ") address first." << std::endl;

                        so.free_func((void*)buf->buffer);
                        return -1;
                    }
                    n = ::sendto(socket_ref.socket_fd, so.buffer, so.sz, 0, &sa.s, sa_sz);
                }
                
                // error
                if (n < 0)
                    n = 0; // ignore error, let socket thread try again

                // 发统计
                socket_ref.stat_send(n, time_);

                // 写完
                if (n == so.sz)
                {
                    so.free_func((void*)buf->buffer);
                    return 0;
                }

                // 直接发送失败, 将buffer加入到s->dw_*, 让socket线程去发送. 参考: send_write_buffer()
                socket_ref.dw_buffer = clone_send_buffer(buf, &socket_ref.dw_size);
                socket_ref.dw_offset = n;

                event_poller_.write(socket_ref.socket_fd, &socket_ref, true);

                return 0;
            }
        }
    }

    //
    socket_ref.inc_sending_ref(socket_id);

    //
    ctrl_cmd_package cmd;
    const send_buffer* clone_buf_ptr = (const send_buffer*)clone_send_buffer(buf, &cmd.u.send.sz);
    prepare_ctrl_cmd_request_send(cmd, socket_id, clone_buf_ptr, true);
    _send_ctrl_cmd(&cmd);

    return 0;
}

int socket_server::send_low_priority(send_buffer* buf)
{
    int socket_id = buf->socket_id;
    auto& socket_ref = socket_slot_[calc_slot_index(socket_id)];

    // 检查socket状态
    if (socket_ref.socket_id != socket_id || socket_ref.status == socket::status::FREE)
    {
        free_send_buffer(buf);
        return -1;
    }

    // 增加发送计数
    socket_ref.inc_sending_ref(socket_id);

    // 
    ctrl_cmd_package cmd;
    const send_buffer* clone_buf_ptr = (const send_buffer*)clone_send_buffer(buf, &cmd.u.send.sz);
    prepare_ctrl_cmd_request_send(cmd, socket_id, clone_buf_ptr, false);
    _send_ctrl_cmd(&cmd);

    return 0;
}

void socket_server::userobject(socket_object_interface* soi)
{
    soi_ = *soi;
}

int socket_server::connect(uint64_t svc_handle, const char* addr, int port)
{
    // 分配一个socket
    int socket_id = _alloc_socket_id();
    if (socket_id < 0)
        return -1;

    ctrl_cmd_package cmd;
    int len = prepare_ctrl_cmd_request_open(cmd, svc_handle, socket_id, addr, port);
    if (len < 0)
        return -1;
    
    _send_ctrl_cmd(&cmd);

    return socket_id;
}

int socket_server::bind(uint64_t svc_handle, int fd)
{
    // 分配一个socket
    int socket_id = _alloc_socket_id();
    if (socket_id < 0)
        return -1;

    //
    ctrl_cmd_package cmd;
    prepare_ctrl_cmd_request_bind(cmd, svc_handle, socket_id, fd);
    _send_ctrl_cmd(&cmd);

    return socket_id;
}

void socket_server::nodelay(int socket_id)
{
    ctrl_cmd_package cmd;
    prepare_ctrl_cmd_request_set_opt(cmd, socket_id);
    _send_ctrl_cmd(&cmd);
}


//----------------------------------------------
// UDP
//----------------------------------------------

int socket_server::socket_server_udp(uint64_t svc_handle, const char* addr, int port)
{
    int fd = INVALID_FD;
    int family;
    if (port != 0 || addr != NULL)
    {
        // bind
        fd = socket_helper::bind(addr, port, IPPROTO_UDP, &family);
        if (fd < 0)
            return -1;
    }
    else
    {
        family = AF_INET;
        fd = ::socket(family, SOCK_DGRAM, 0);
        if (fd < 0)
            return -1;
    }
    socket_helper::nonblocking(fd);

    int socket_id = _alloc_socket_id();
    if (socket_id < 0)
    {
        ::close(fd);
        return -1;
    }

    ctrl_cmd_package cmd;
    prepare_ctrl_cmd_request_udp(cmd, svc_handle, socket_id, fd, family);
    _send_ctrl_cmd(&cmd);

    return socket_id;
}

int socket_server::udp_send(const socket_udp_address* addr, send_buffer* buf)
{
    int socket_id = buf->socket_id;
    auto& socket_ref = socket_slot_[calc_slot_index(socket_id)];
    if (socket_ref.socket_id != socket_id || socket_ref.status == socket::status::FREE)
    {
        free_send_buffer(buf);
        return -1;
    }

    const uint8_t* udp_address = (const uint8_t*)addr;
    int addr_sz;
    switch (udp_address[0])
    {
    case protocol_type::UDP:
        addr_sz = 1 + 2 + 4;    // 1 type, 2 port, 4 ipv4
        break;
    case protocol_type::UDPv6:
        addr_sz = 1 + 2 + 16;   // 1 type, 2 port, 16 ipv6
        break;
    default:
        free_send_buffer(buf);
        return -1;
    }

    if (socket_ref.can_direct_write(socket_id))
    {
        // scope lock
        std::unique_lock<std::mutex> sl(socket_ref.dw_mutex);

        if (sl.try_lock())
        {
            // may be we can send directly, double check
            if (socket_ref.can_direct_write(socket_id))
            {
                // send directly
                send_object so;
                send_object_init(&so, buf);
                socket_addr sa;
                socklen_t sa_sz = udp_address_to_socket_addr(socket_ref.protocol, udp_address, sa);
                if (sa_sz == 0)
                {
                    so.free_func((void*)buf->buffer);
                    return -1;
                }

                int n = ::sendto(socket_ref.socket_fd, so.buffer, so.sz, 0, &sa.s, sa_sz);
                if (n >= 0)
                {
                    // sendto succ
                    socket_ref.stat_send(n, time_);
                    so.free_func((void*)buf->buffer);
                    return 0;
                }
            }
            // else: let socket thread try again, udp doesn't care the order
        }
    }

    ctrl_cmd_package cmd;
    send_buffer* clone_buf = (send_buffer*)clone_send_buffer(buf, &cmd.u.send_udp.send.sz);
    prepare_ctrl_cmd_request_send_udp(cmd, socket_id, clone_buf, udp_address, addr_sz);
    _send_ctrl_cmd(&cmd);

    return 0;
}

int socket_server::udp_connect(int socket_id, const char* addr, int port)
{
    auto& socket_ref = socket_slot_[calc_slot_index(socket_id)];
    if (socket_ref.socket_id != socket_id || socket_ref.status == socket::status::FREE)
        return -1;

    // increase udp_connecting, use scope lock
    {
        std::lock_guard<std::mutex> lock(socket_ref.dw_mutex);

        if (socket_ref.socket_id != socket_id || socket_ref.status == socket::status::FREE)
            return -1;
        
        ++socket_ref.udp_connecting;
    }

    addrinfo ai_hints;
    ::memset(&ai_hints, 0, sizeof(ai_hints));
    ai_hints.ai_family = AF_UNSPEC;
    ai_hints.ai_socktype = SOCK_DGRAM;
    ai_hints.ai_protocol = IPPROTO_UDP;

    char port_str[16];
    sprintf(port_str, "%d", port);    

    addrinfo* ai_list = nullptr;

    int status = ::getaddrinfo(addr, port_str, &ai_hints, &ai_list);
    if (status != 0)
        return -1;

    int protocol;
    if (ai_list->ai_family == AF_INET)
    {
        protocol = protocol_type::UDP;
    }
    else if (ai_list->ai_family == AF_INET6)
    {
        protocol = protocol_type::UDPv6;
    }
    else
    {
        ::freeaddrinfo(ai_list);
        return -1;
    }

    ctrl_cmd_package cmd;
    prepare_ctrl_cmd_request_set_udp(cmd, socket_id, protocol, (socket_addr*)ai_list->ai_addr);
    ::freeaddrinfo(ai_list);
    _send_ctrl_cmd(&cmd);

    return 0;
}

const socket_udp_address* socket_server::udp_address(socket_message* msg, int* addr_sz)
{
    // type
    uint8_t* address = (uint8_t*)(msg->data + msg->ud);
    int type = address[0];

    // 必须为udp
    if (type == protocol_type::UDP)
        *addr_sz = 1 + 2 + 4;
    else if (type == protocol_type::UDPv6)
        *addr_sz = 1 + 2 + 16;
    else
        return nullptr;

    return (const socket_udp_address*)address;
}

socket_info* socket_server::get_socket_info()
{
    socket_info* si = nullptr;
    for (int i=0; i<MAX_SOCKET; i++)
    {
        auto& socket_ref = socket_slot_[i];
        int socket_id = socket_ref.socket_id;
        socket_info tmp_info;

        // 该方法可以在不同的线程中调用, 再次检查下id
        if (_query_socket_info(socket_ref, tmp_info) && socket_ref.socket_id == socket_id)
        {
            si = socket_info::create(si);
            tmp_info.next = si->next;
            *si = tmp_info;
        }
    }

    return si;
}

//----------------------------------------------
// ctrl cmd
//----------------------------------------------

void socket_server::_send_ctrl_cmd(ctrl_cmd_package* cmd)
{
    // header部分的长度为数据长度, 不header部分
    // header部分为2字节, type: 1 byte, data_len: 1 byte
    int data_len = cmd->header[7];

    for (;;)
    {
        // 写数据到pipe
        int n = pipe_.write((const char*)&cmd->header[6], data_len + 2);

        // 写失败了, 重试
        if (n < 0)
        {
            if (errno != EINTR)
            {
                std::cerr << "socket-server : send ctrl command error " << ::strerror(errno) << "." << std::endl;
            }
            
            continue;
        }

        // write success
        assert(n == data_len + 2);
        return;
    }
}

// 当工作线程执行socket.listen后，socket线程从接收管道读取数据，执行ctrl_cmd
int socket_server::_recv_ctrl_cmd(socket_message* result)
{
    // recv header: type (1 byte) + len (1 byte)
    uint8_t header[2] = { 0 };
    if (pipe_.read((char*)header, 2) == -1)
    {
        std::cerr << "socket-server : read pipe error " << ::strerror(errno) << "." << std::endl;
        return -1;
    }
    int ctrl_cmd = header[0];
    int len = header[1];

    // recv data
    uint8_t buf[256] = { 0 };
    if (pipe_.read((char*)buf, len) == -1)
    {
        std::cerr << "socket-server : read pipe error " << ::strerror(errno) << "." << std::endl;
        return -1;
    }

    // 处理
    switch (ctrl_cmd)
    {
    case 'S':
        return handle_ctrl_cmd_start_socket((request_start*)buf, result);
    case 'B':
        return handle_ctrl_cmd_bind_socket((request_bind*)buf, result);
    case 'L':
        return handle_ctrl_cmd_listen_socket((request_listen*)buf, result);
    case 'K':
        return handle_ctrl_cmd_close_socket((request_close*)buf, result);
    case 'O':
        return handle_ctrl_cmd_open_socket((request_open*)buf, result);
    case 'X':
        return handle_ctrl_cmd_exit_socket(result);
    case 'D':
    case 'P': 
    {
        int priority = (ctrl_cmd == 'D') ? priority_type::HIGH : priority_type::LOW;
        auto cmd = (request_send*)buf;
        int ret = handle_ctrl_cmd_send_socket(cmd, result, priority, nullptr);
        
        auto& socket_ref = socket_slot_[calc_slot_index(cmd->socket_id)];
        socket_ref.dec_sending_ref(cmd->socket_id);
        
        return ret;
    }
    case 'A': 
    {
        auto cmd = (request_send_udp*)buf;
        return handle_ctrl_cmd_send_socket(&cmd->send, result, priority_type::HIGH, cmd->address);
    }
    case 'C':
        return handle_ctrl_cmd_set_udp_address((request_set_udp*)buf, result);
    case 'T':
        return handle_ctrl_cmd_setopt_socket((request_set_opt*)buf);
    case 'U':
        return handle_ctrl_cmd_add_udp_socket((request_udp*)buf);
    }

    // 
    std::cerr << "socket-server : Unknown ctrl command " << ctrl_cmd << "." << std::endl;
    return -1;
}

// return -1 when connecting
int socket_server::handle_ctrl_cmd_open_socket(request_open* cmd, socket_message* result)
{
    int socket_id = cmd->socket_id;

    result->svc_handle = cmd->svc_handle;
    result->socket_id = socket_id;
    result->ud = 0;
    result->data = nullptr;

    addrinfo ai_hints;
    ::memset(&ai_hints, 0, sizeof(ai_hints));
    ai_hints.ai_family = AF_UNSPEC;
    ai_hints.ai_socktype = SOCK_STREAM;
    ai_hints.ai_protocol = IPPROTO_TCP;

    addrinfo* ai_list = nullptr;

    bool is_ok = false;
    do
    {
        char port[16] = { 0 };
        sprintf(port, "%d", cmd->port);
        int status = ::getaddrinfo(cmd->host, port, &ai_hints, &ai_list);
        if (status != 0)
        {
            result->data = const_cast<char*>(::gai_strerror(status));
            break;  // failed
        }

        int sock_fd = INVALID_FD;
        addrinfo* ai_ptr = nullptr;
        for (ai_ptr = ai_list; ai_ptr != nullptr; ai_ptr = ai_ptr->ai_next)
        {
            sock_fd = ::socket(ai_ptr->ai_family, ai_ptr->ai_socktype, ai_ptr->ai_protocol);
            if (sock_fd < 0)
                continue;

            socket_helper::keepalive(sock_fd);
            socket_helper::nonblocking(sock_fd);
            status = ::connect(sock_fd, ai_ptr->ai_addr, ai_ptr->ai_addrlen);
            if (status != 0 && errno != EINPROGRESS)
            {
                ::close(sock_fd);
                sock_fd = INVALID_FD;
                continue;
            }
            break;
        }

        // failed
        if (sock_fd < 0)
        {
            result->data = ::strerror(errno);
            break;
        }

        //
        socket* new_socket_ptr = new_socket(socket_id, sock_fd, protocol_type::TCP, cmd->svc_handle);
        if (new_socket_ptr == nullptr)
        {
            ::close(sock_fd);
            result->data = const_cast<char*>("reach skynet socket number limit");
            break;
        }

        if (status == 0)
        {
            new_socket_ptr->status = socket::status::CONNECTED;
            sockaddr* addr = ai_ptr->ai_addr;
            void* sin_addr = (ai_ptr->ai_family == AF_INET) ? (void*)&((sockaddr_in*)addr)->sin_addr : (void*)&((sockaddr_in6*)addr)->sin6_addr;
            if (::inet_ntop(ai_ptr->ai_family, sin_addr, addr_tmp_buf_, ADDR_TMP_BUFFER_SIZE))
            {
                result->data = addr_tmp_buf_;
            }
            ::freeaddrinfo(ai_list);
            return socket_event::SOCKET_OPEN;
        }
        else
        {
            new_socket_ptr->status = socket::status::CONNECTING;
            event_poller_.write(new_socket_ptr->socket_fd, new_socket_ptr, true);
        }

        is_ok = true;
    } while (0);

    // failed
    if (!is_ok)
    {
        ::freeaddrinfo(ai_list);
        socket_slot_[calc_slot_index(socket_id)].status = socket::status::FREE;
        return socket_event::SOCKET_ERROR;
    }

    // success
    ::freeaddrinfo(ai_list);
    return -1;
}

int socket_server::handle_ctrl_cmd_close_socket(request_close* cmd, socket_message* result)
{
    int socket_id = cmd->socket_id;
    auto& socket_ref = socket_slot_[calc_slot_index(socket_id)];
    if (socket_ref.status == socket::status::FREE || socket_ref.socket_id != socket_id)
    {
        result->socket_id = socket_id;
        result->svc_handle = cmd->svc_handle;
        result->ud = 0;
        result->data = nullptr;
        return socket_event::SOCKET_CLOSE;
    }

    socket_lock sl(socket_ref.dw_mutex);

    //
    if (!socket_ref.nomore_sending_data())
    {
        int type = send_write_buffer(&socket_ref, sl, result);

        // -1 or socket_event::SOCKET_WARNING or socket_event::SOCKET_CLOSE, 
        //       socket_event::SOCKET_WARNING means nomore_sending_data
        if (type != -1 && type != socket_event::SOCKET_WARNING)
            return type;
    }

    //
    if (cmd->shutdown == 1 || socket_ref.nomore_sending_data())
    {
        force_close(&socket_ref, sl, result);

        result->socket_id = socket_id;
        result->svc_handle = cmd->svc_handle;
        return socket_event::SOCKET_CLOSE;
    }
    
    socket_ref.status = socket::status::HALF_CLOSE;

    return -1;
}

int socket_server::handle_ctrl_cmd_bind_socket(request_bind* cmd, socket_message* result)
{
    int socket_id = cmd->socket_id;
    result->socket_id = socket_id;
    result->svc_handle = cmd->svc_handle;
    result->ud = 0;

    //
    socket* new_socket_ptr = new_socket(socket_id, cmd->fd, protocol_type::TCP, cmd->svc_handle);
    if (new_socket_ptr == nullptr)
    {
        result->data = const_cast<char*>("reach skynet socket number limit");
        return socket_event::SOCKET_ERROR;
    }

    socket_helper::nonblocking(cmd->fd);
    new_socket_ptr->status = socket::status::BIND;
    result->data = const_cast<char*>("binding");

    return socket_event::SOCKET_OPEN;
}

int socket_server::handle_ctrl_cmd_start_socket(request_start* cmd, socket_message* result)
{
    int socket_id = cmd->socket_id;

    //
    result->socket_id = socket_id;
    result->svc_handle = cmd->svc_handle;
    result->ud = 0;
    result->data = nullptr;

    auto& socket_ref = socket_slot_[calc_slot_index(socket_id)];
    if (socket_ref.status == socket::status::FREE || socket_ref.socket_id != socket_id)
    {
        result->data = const_cast<char*>("invalid socket");
        return socket_event::SOCKET_ERROR;
    }
    
    //
    if (socket_ref.status == socket::status::PACCEPT || socket_ref.status == socket::status::PLISTEN)
    {
        if (!event_poller_.add(socket_ref.socket_fd, &socket_ref))
        {
            socket_lock sl(socket_ref.dw_mutex);
            force_close(&socket_ref, sl, result);

            result->data = ::strerror(errno);
            return socket_event::SOCKET_ERROR;
        }

        socket_ref.status = (socket_ref.status == socket::status::PACCEPT) ? socket::status::CONNECTED : socket::status::LISTEN;
        socket_ref.svc_handle = cmd->svc_handle;
        result->data = const_cast<char*>("start");
        return socket_event::SOCKET_OPEN;
    }
    //
    else if (socket_ref.status == socket::status::CONNECTED)
    {
        // todo: maybe we should send a message SOCKET_TRANSFER to socket_ptr->svc_handle
        socket_ref.svc_handle = cmd->svc_handle;
        result->data = const_cast<char*>("transfer");
        return socket_event::SOCKET_OPEN;
    }

    // if socket_ptr->status == socket::status::HALF_CLOSE , socket::status::SOCKET_CLOSE message will send later
    return -1;
}

int socket_server::handle_ctrl_cmd_setopt_socket(request_set_opt* cmd)
{
    int socket_id = cmd->socket_id;

    auto& socket_ref = socket_slot_[calc_slot_index(socket_id)];
    if (socket_ref.status == socket::status::FREE || socket_ref.socket_id != socket_id)
        return socket_event::SOCKET_ERROR;

    int v = cmd->value;
    ::setsockopt(socket_ref.socket_fd, IPPROTO_TCP, cmd->what, &v, sizeof(v));

    return -1;
}

int socket_server::handle_ctrl_cmd_exit_socket(socket_message* result)
{
    result->svc_handle = 0;
    result->socket_id = 0;
    result->ud = 0;
    result->data = nullptr;

    return socket_event::SOCKET_EXIT;
}

/**
 * 发送数据
 * 可以设置发送优先级: priority_type::HIGH 或 priority_type::LOW
 * 
 * 如果socket缓存为空, 直接将数据写入fd中.
 * 如果是写入部分数据, 将剩余部分写入到 高优先级 列表中. (即使优先级为 priority_type::LOW 也是如此)
 * 否则, 将数据添加到高优先级队列(priority_type::HIGH) 或 低优先级队列(priority_type::LOW).
 */
int socket_server::handle_ctrl_cmd_send_socket(request_send* cmd, socket_message* result, int priority, const uint8_t* udp_address)
{
    int socket_id = cmd->socket_id;
    send_object so;
    send_object_init(&so, cmd->data_ptr, cmd->sz);

    //
    auto& socket_ref = socket_slot_[calc_slot_index(socket_id)];
    if (socket_ref.status == socket::status::FREE || socket_ref.socket_id != socket_id ||
        socket_ref.status == socket::status::HALF_CLOSE ||
        socket_ref.status == socket::status::PACCEPT)
    {
        so.free_func((void*)cmd->data_ptr);
        return -1;
    }
    //
    if (socket_ref.status == socket::status::PLISTEN || socket_ref.status == socket::status::LISTEN)
    {
        std::cerr << "socket-server : write to listen " << socket_id << "." << std::endl;
        so.free_func((void*)cmd->data_ptr);
        return -1;
    }

    // socket已连接
    if (socket_ref.is_send_buffer_empty() && socket_ref.status == socket::status::CONNECTED)
    {
        // tcp
        if (socket_ref.protocol == protocol_type::TCP)
        {
            // add to high priority write_buffer_list, even priority == priority_type::LOW
            append_sendbuffer(&socket_ref, cmd);
        }
        // udp
        else
        {
            if (udp_address == nullptr)
                udp_address = socket_ref.p.udp_address;

            socket_addr sa;
            socklen_t sa_sz = udp_address_to_socket_addr(socket_ref.protocol, udp_address, sa);
            if (sa_sz == 0)
            {
                // udp type mismatch, just drop it.
                std::cerr << "socket-server : udp (" << socket_id << ") type mistach." << std::endl;
                so.free_func((void*)cmd->data_ptr);
                return -1;
            }

            //
            int n = ::sendto(socket_ref.socket_fd, so.buffer, so.sz, 0, &sa.s, sa_sz);
            if (n != so.sz)
            {
                append_sendbuffer(&socket_ref, cmd, priority == priority_type::HIGH, udp_address);
            }
            else
            {
                socket_ref.stat_send(n, time_);
                so.free_func((void*)cmd->data_ptr);
                return -1;
            }
        }
        event_poller_.write(socket_ref.socket_fd, &socket_ref, true);
    }
    //
    else
    {
        // tcp
        if (socket_ref.protocol == protocol_type::TCP)
        {
            append_sendbuffer(&socket_ref, cmd, priority == priority_type::HIGH);
        }
        // udp
        else
        {
            if (udp_address == nullptr)
                udp_address = socket_ref.p.udp_address;

            append_sendbuffer(&socket_ref, cmd, priority == priority_type::HIGH, udp_address);
        }
    }
    if (socket_ref.wb_size >= WARNING_SIZE && socket_ref.wb_size >= socket_ref.warn_size)
    {
        socket_ref.warn_size = socket_ref.warn_size == 0 ? WARNING_SIZE * 2 : socket_ref.warn_size * 2;
        result->svc_handle = socket_ref.svc_handle;
        result->socket_id = socket_ref.socket_id;
        result->ud = socket_ref.wb_size % 1024 == 0 ? socket_ref.wb_size / 1024 : socket_ref.wb_size / 1024 + 1;
        result->data = nullptr;
        return socket_event::SOCKET_WARNING;
    }
    
    return -1;
}

int socket_server::handle_ctrl_cmd_listen_socket(request_listen* cmd, socket_message* result)
{
    int socket_id = cmd->socket_id;
    int listen_fd = cmd->fd;

    // 
    socket* new_socket_ptr = new_socket(socket_id, listen_fd, protocol_type::TCP, cmd->svc_handle, false);
    if (new_socket_ptr == nullptr)
    {
        ::close(listen_fd);
        result->svc_handle = cmd->svc_handle;
        result->socket_id = socket_id;
        result->ud = 0;
        result->data = const_cast<char*>("reach socket number limit");
        socket_slot_[calc_slot_index(socket_id)].status = socket::status::FREE;

        return socket_event::SOCKET_ERROR;
    }
    
    new_socket_ptr->status = socket::status::PLISTEN;

    return -1;
}

int socket_server::handle_ctrl_cmd_add_udp_socket(request_udp* cmd)
{
    int socket_id = cmd->socket_id;
    int protocol = cmd->family == AF_INET6 ? protocol_type::UDPv6 : protocol_type::UDP;

    socket* new_socket_ptr = new_socket(socket_id, cmd->fd, protocol, cmd->svc_handle);
    if (new_socket_ptr == nullptr)
    {
        ::close(cmd->fd);
        socket_slot_[calc_slot_index(socket_id)].status = socket::status::FREE;
        return socket_event::SOCKET_ERROR;
    }

    new_socket_ptr->status = socket::status::CONNECTED;
    ::memset(new_socket_ptr->p.udp_address, 0, sizeof(new_socket_ptr->p.udp_address));

    return -1;
}

int socket_server::handle_ctrl_cmd_set_udp_address(request_set_udp* cmd, socket_message* result)
{
    int socket_id = cmd->socket_id;
    auto& socket_ref = socket_slot_[calc_slot_index(socket_id)];
    if (socket_ref.status == socket::status::FREE || socket_ref.socket_id !=socket_id)
        return -1;

    //
    int type = cmd->address[0];
    if (type != socket_ref.protocol)
    {
        // protocol mismatch
        result->svc_handle = socket_ref.svc_handle;
        result->socket_id = socket_ref.socket_id;
        result->ud = 0;
        result->data = const_cast<char*>("protocol mismatch");

        return socket_event::SOCKET_ERROR;
    }

    if (type == protocol_type::UDP)
        ::memcpy(socket_ref.p.udp_address, cmd->address, 1 + 2 + 4);     // 1 type, 2 port, 4 ipv4
    else
        ::memcpy(socket_ref.p.udp_address, cmd->address, 1 + 2 + 16);    // 1 type, 2 port, 16 ipv6

    --socket_ref.udp_connecting;
    
    return -1;
}

//----------------------------------------------
// private
//----------------------------------------------

void socket_server::_clear_closed_event(int socket_id)
{
    for (int i = event_next_index_; i < event_wait_n_; i++)
    {
        auto& event_ref = events_[i];
        auto socket_ptr = (socket*)event_ref.socket_ptr;

        // 跳过无效的
        if (socket_ptr == nullptr || socket_ptr->socket_id != socket_id)
            continue;

        // 找到给定的socket, 当前为空闲状态, 需要清理掉
        if (socket_ptr->status == socket::status::FREE)
        {
            event_ref.socket_ptr = nullptr;
            break;
        }
    }
}

void socket_server::force_close(socket* socket_ptr, socket_lock& sl, socket_message* result)
{
    //
    result->svc_handle = socket_ptr->svc_handle;
    result->socket_id = socket_ptr->socket_id;
    result->ud = 0;
    result->data = nullptr;

    //
    if (socket_ptr->status == socket::status::FREE)
        return;

    assert(socket_ptr->status != socket::status::ALLOCED);
    free_write_buffer_list(&socket_ptr->wb_list_high);
    free_write_buffer_list(&socket_ptr->wb_list_low);
    if (socket_ptr->status != socket::status::PACCEPT && socket_ptr->status != socket::status::PLISTEN)
    {
        event_poller_.del(socket_ptr->socket_fd);
    }
    
    sl.lock();
    if (socket_ptr->status != socket::status::BIND)
    {
        if (::close(socket_ptr->socket_fd) < 0)
            perror("close socket:");
    }
    socket_ptr->status = socket::status::FREE;
    if (socket_ptr->dw_buffer != nullptr)
    {
        send_buffer tmp;
        tmp.buffer = socket_ptr->dw_buffer;
        tmp.sz = socket_ptr->dw_size;
        tmp.socket_id = socket_ptr->socket_id;
        tmp.type = (tmp.sz == USER_OBJECT) ? buffer_type::OBJECT : buffer_type::MEMORY;
        free_send_buffer(&tmp);
        socket_ptr->dw_buffer = nullptr;
    }
    sl.unlock();
}

int socket_server::_alloc_socket_id()
{
    for (int i = 0; i < MAX_SOCKET; i++)
    {
        int socket_id = ++alloc_socket_id_;
        
        // overflow
        if (socket_id < 0)
        {
            alloc_socket_id_ &= 0x7FFFFFFF;
            socket_id = alloc_socket_id_;
        }

        // 
        auto& socket_ref = socket_slot_[calc_slot_index(socket_id)];
        
        // socket is not available
        if (socket_ref.status != socket::status::FREE)
            continue;

        // set socket status: alloced
        uint8_t expect_status = socket::status::FREE;
        if (socket_ref.status.compare_exchange_strong(expect_status, socket::status::ALLOCED))
        {
            socket_ref.socket_id = socket_id;
            socket_ref.protocol = protocol_type::UNKNOWN;
            socket_ref.udp_connecting = 0;  // socket_server::udp_connect 可以直接增加 socket_ref.udp_conncting (在其他线程, new_fd之前), 因此这里重置为0
            socket_ref.socket_fd = INVALID_FD;
            return socket_id;
        }
        // socket status change before set, retry
        else
        {
            --i;
        }
    }
    
    return -1;
}

void socket_server::drop_udp(socket* socket_ptr, write_buffer_list* wb_list, write_buffer* wb)
{
    socket_ptr->wb_size -= wb->sz;
    wb_list->head = wb->next;
    if (wb_list->head == nullptr)
        wb_list->tail = nullptr;
    free_write_buffer(wb);
}

socket* socket_server::new_socket(int socket_id, int sock_fd, int protocol, uint64_t svc_handle, bool add/* = true*/)
{
    auto& socket_ref = socket_slot_[calc_slot_index(socket_id)];
    assert(socket_ref.status == socket::status::ALLOCED);

    // add to event poller
    if (add && !event_poller_.add(sock_fd, &socket_ref))
    {
        socket_ref.status = socket::status::FREE;
        return nullptr;
    }

    // 
    socket_ref.socket_id = socket_id;
    socket_ref.socket_fd = sock_fd;
    socket_ref.sending = socket_id_tag16(socket_id) << 16 | 0; // high 16 bits: socket id, low 16 bits: actually sending count
    socket_ref.protocol = protocol;
    socket_ref.p.size = MIN_READ_BUFFER;
    socket_ref.svc_handle = svc_handle;
    socket_ref.wb_size = 0;
    socket_ref.warn_size = 0;

    // check write_buffer_list
    assert(socket_ref.wb_list_high.head == nullptr);
    assert(socket_ref.wb_list_high.tail == nullptr);
    assert(socket_ref.wb_list_low.head == nullptr);
    assert(socket_ref.wb_list_low.tail == nullptr);    

    //
    socket_ref.dw_buffer = nullptr;
    socket_ref.dw_size = 0;

    //
    ::memset(&socket_ref.stat, 0, sizeof(socket_ref.stat));

    return &socket_ref;
}

void socket_server::free_send_buffer(send_buffer* buf_ptr)
{
    if (buf_ptr == nullptr)
        return;

    char* buffer = (char*)buf_ptr->buffer;
    if (buf_ptr->type == buffer_type::MEMORY)
    {
        delete[] buffer;
    }
    else if (buf_ptr->type == buffer_type::OBJECT)
    {
        soi_.free(buffer);
    }
    else if (buf_ptr->type == buffer_type::RAW_POINTER)
    {
        //
    }
}

const void* socket_server::clone_send_buffer(send_buffer* buf_ptr, size_t *sz)
{
    if (buf_ptr == nullptr)
    {
        *sz = 0;
        return nullptr;
    }
    
    if (buf_ptr->type == buffer_type::MEMORY)
    {
        *sz = buf_ptr->sz;
        return buf_ptr->buffer;
    }
    else if (buf_ptr->type == buffer_type::OBJECT)
    {
        *sz = USER_OBJECT;
        return buf_ptr->buffer;
    }
    // It's a raw pointer, we need make a copy
    else if (buf_ptr->type == buffer_type::RAW_POINTER)
    {
        *sz = buf_ptr->sz;
        void* tmp = new char[*sz];
        ::memcpy(tmp, buf_ptr->buffer, *sz);
        return tmp;
    }
    // never get here
    else
    {
        *sz = 0;
        return nullptr;
    }
}

void socket_server::append_sendbuffer(socket* socket_ptr, request_send* cmd, bool is_high/* = true*/, const uint8_t* udp_address/* = nullptr*/)
{
    auto wb_list= is_high ? &socket_ptr->wb_list_high : &socket_ptr->wb_list_low;
    auto write_buf_ptr = prepare_write_buffer(wb_list, cmd, udp_address == nullptr ? SIZEOF_TCP_BUFFER : SIZEOF_UDP_BUFFER);

    // append udp address
    if (udp_address != nullptr)
        ::memcpy(write_buf_ptr->udp_address, udp_address, UDP_ADDRESS_SIZE);

    // set write buffer size
    socket_ptr->wb_size += write_buf_ptr->sz;
}

//
write_buffer* socket_server::prepare_write_buffer(write_buffer_list* wb_list, request_send* cmd, int size)
{
    //
    auto write_buf_ptr = (write_buffer*)new char[size]{0};

    //
    send_object so;
    write_buf_ptr->is_userobject = send_object_init(&so, cmd->data_ptr, cmd->sz);
    write_buf_ptr->ptr = (char*)so.buffer;
    write_buf_ptr->sz = so.sz;
    write_buf_ptr->buffer = cmd->data_ptr;
    write_buf_ptr->next = nullptr;
    if (wb_list->head == nullptr)
    {
        wb_list->head = wb_list->tail = write_buf_ptr;
    }
    else
    {
        assert(wb_list->tail != nullptr);
        assert(wb_list->tail->next == nullptr);

        wb_list->tail->next = write_buf_ptr;
        wb_list->tail = write_buf_ptr;
    }

    return write_buf_ptr;
}

void socket_server::free_write_buffer(write_buffer* wb)
{
    if (wb->is_userobject)
    {
//         soi_.free((void*)wb->buffer);
        delete[] (char*)wb->buffer;
    }
    else
    {
        delete[] (char*)wb->buffer;
    }

    delete wb;
}

void socket_server::free_write_buffer_list(write_buffer_list* wb_list)
{
    write_buffer* wb = wb_list->head;
    while (wb != nullptr)
    {
        write_buffer* tmp = wb;
        wb = wb->next;
        free_write_buffer(tmp);
    }
    wb_list->head = nullptr;
    wb_list->tail = nullptr;
}

//
int socket_server::send_write_buffer(socket* socket_ptr, socket_lock& sl, socket_message* result)
{
    // blocked by direct write, 稍后再发
    if (!sl.try_lock())
        return -1;

    if (socket_ptr->dw_buffer != nullptr)
    {
        // add direct write buffer before high.head
        auto write_buf_ptr = (write_buffer*)new char[SIZEOF_TCP_BUFFER]{0};

        send_object so;
        write_buf_ptr->is_userobject = send_object_init(&so, (void*)socket_ptr->dw_buffer, socket_ptr->dw_size);
        write_buf_ptr->ptr = (char*)so.buffer + socket_ptr->dw_offset;
        write_buf_ptr->sz = so.sz - socket_ptr->dw_offset;
        write_buf_ptr->buffer = (void*)socket_ptr->dw_buffer;
        socket_ptr->wb_size += write_buf_ptr->sz;
        if (socket_ptr->wb_list_high.head == nullptr)
        {
            socket_ptr->wb_list_high.head = socket_ptr->wb_list_high.tail = write_buf_ptr;
            write_buf_ptr->next = nullptr;
        }
        else
        {
            write_buf_ptr->next = socket_ptr->wb_list_high.head;
            socket_ptr->wb_list_high.head = write_buf_ptr;
        }
        socket_ptr->dw_buffer = nullptr;
    }

    //
    int type = do_send_write_buffer(socket_ptr, sl, result);

    //
    sl.unlock();

    return type;
}


int socket_server::send_write_buffer_list(socket* socket_ptr, write_buffer_list* wb_list, socket_lock& sl, socket_message* result)
{
    if (socket_ptr->protocol == protocol_type::TCP)
        return send_write_buffer_list_tcp(socket_ptr, wb_list, sl, result);
    else
        return send_write_buffer_list_udp(socket_ptr, wb_list, result);
}

int socket_server::send_write_buffer_list_tcp(socket* socket_ptr, write_buffer_list* wb_list, socket_lock& sl, socket_message* result)
{
    while (wb_list->head != nullptr)
    {
        auto tmp = wb_list->head;
        for (;;)
        {
            ssize_t sz = ::write(socket_ptr->socket_fd, tmp->ptr, tmp->sz);
            if (sz < 0)
            {
                //
                if (errno == EINTR)
                    continue;

                //
                if (errno == AGAIN_WOULDBLOCK)
                    return -1;

                force_close(socket_ptr, sl, result);
                return socket_event::SOCKET_CLOSE;
            }
            
            socket_ptr->stat_send((int)sz, time_);
            socket_ptr->wb_size -= sz;
            if (sz != tmp->sz)
            {
                tmp->ptr += sz;
                tmp->sz -= sz;
                return -1;
            }
            
            break;
        }
        wb_list->head = tmp->next;
        free_write_buffer(tmp);
    }
    wb_list->tail = nullptr;

    return -1;
}


int socket_server::send_write_buffer_list_udp(socket* socket_ptr, write_buffer_list* wb_list, socket_message* result)
{
    while (wb_list->head != nullptr)
    {
        auto tmp = wb_list->head;
        socket_addr sa;
        socklen_t sa_sz = udp_address_to_socket_addr(socket_ptr->protocol, tmp->udp_address, sa);
        if (sa_sz == 0)
        {
            std::cerr << "socket-server : udp (" << socket_ptr->socket_id << ") type mismatch." << std::endl;
            drop_udp(socket_ptr, wb_list, tmp);
            return -1;
        }
        
        // 发送数据
        int err = ::sendto(socket_ptr->socket_fd, tmp->ptr, tmp->sz, 0, &sa.s, sa_sz);
        if (err < 0)
        {
            //
            if (errno == EINTR || errno == AGAIN_WOULDBLOCK)
                return -1;

            std::cerr << "socket-server : udp (" << socket_ptr->socket_id << ") sendto error " << ::strerror(errno) << "." << std::endl;
            drop_udp(socket_ptr, wb_list, tmp);
            return -1;
        }

        //
        socket_ptr->stat_send(tmp->sz, time_);

        //
        socket_ptr->wb_size -= tmp->sz;
        wb_list->head = tmp->next;
        free_write_buffer(tmp);
    }
    wb_list->tail = nullptr;

    return -1;
}

int socket_server::list_uncomplete(write_buffer_list* wb_list)
{
    auto write_buf_ptr = wb_list->head;
    if (write_buf_ptr == nullptr)
        return 0;

    return (void*)write_buf_ptr->ptr != write_buf_ptr->buffer;
}

void socket_server::raise_uncomplete(socket* socket_ptr)
{
    auto wb_list_low = &socket_ptr->wb_list_low;
    auto tmp = wb_list_low->head;
    wb_list_low->head = tmp->next;
    if (wb_list_low->head == nullptr)
        wb_list_low->tail = nullptr;

    // move head of low write_buffer_list (tmp) to the empty high write_buffer_list
    auto wb_list_high = &socket_ptr->wb_list_high;
    assert(wb_list_high->head == nullptr);

    tmp->next = nullptr;
    wb_list_high->head = wb_list_high->tail = tmp;
}

/**
 * socket线程最终通过该函数发送数据
 * 每个socket都有两个写缓存列表: '高优先级' 和 '低优先级' 写缓存列表
 * 
 * 发送数据逻辑:
 * 1. 优先发送 '高优先级'写缓存列表内的数据;
 * 2. 若 '高优先级'写缓存列表为空, 发送 '低优先级'写缓存列表内的数据;
 * 3. 若 '低优先级'写缓存列表内的数据是不完整的 (write_buffer_list head不完整, 之前发送了部分数据), 将 '低优先级'写缓存列表的head移到空'高优先级'写缓存队列内(调用raise_uncomplete);
 * 4. 如果两个写缓存队列都为空, 重新加入到epoll事件里? turn off the event. (调用 check_close)
 */
int socket_server::do_send_write_buffer(socket* socket_ptr, socket_lock& sl, socket_message* result)
{
    assert(list_uncomplete(&socket_ptr->wb_list_low) == 0);

    // step 1
    if (send_write_buffer_list(socket_ptr, &socket_ptr->wb_list_high, sl, result) == socket_event::SOCKET_CLOSE)
        return socket_event::SOCKET_CLOSE;

    //
    if (socket_ptr->wb_list_high.head == nullptr)
    {
        // step 2
        if (socket_ptr->wb_list_low.head != nullptr)
        {
            if (send_write_buffer_list(socket_ptr, &socket_ptr->wb_list_low, sl, result) == socket_event::SOCKET_CLOSE)
                return socket_event::SOCKET_CLOSE;

            // step 3
            if (list_uncomplete(&socket_ptr->wb_list_low) != 0)
            {
                raise_uncomplete(socket_ptr);
                return -1;
            }
            if (socket_ptr->wb_list_low.head != nullptr)
                return -1;
        }

        // step 4
        assert(socket_ptr->is_send_buffer_empty() && socket_ptr->wb_size == 0);
        event_poller_.write(socket_ptr->socket_fd, socket_ptr, false);

        if (socket_ptr->status == socket::status::HALF_CLOSE)
        {
            force_close(socket_ptr, sl, result);
            return socket_event::SOCKET_CLOSE;
        }
        if(socket_ptr->warn_size > 0)
        {
            socket_ptr->warn_size = 0;
            result->svc_handle = socket_ptr->svc_handle;
            result->socket_id = socket_ptr->socket_id;
            result->ud = 0;
            result->data = nullptr;
            return socket_event::SOCKET_WARNING;
        }
    }

    return -1;
}

int socket_server::report_accept(socket* socket_ptr, socket_message* result)
{
    // wait accept
    socket_addr sa;
    socklen_t sa_sz = sizeof(sa);
    int client_fd = ::accept(socket_ptr->socket_fd, &sa.s, &sa_sz);

    // accept client failed
    if (client_fd < 0)
    {
        // '文件描述符达到上限' 错误
        if (errno != EMFILE && errno != ENFILE)
            return 0;

        //
        result->svc_handle = socket_ptr->svc_handle;
        result->socket_id = socket_ptr->socket_id;
        result->ud = 0;
        result->data = ::strerror(errno);
        return -1;
    }

    // alloc a new socket id
    int socket_id = _alloc_socket_id();
    if (socket_id < 0)
    {
        ::close(client_fd);
        return 0;
    }

    // set socket option: 'keepalive' & 'nonblocking'
    socket_helper::keepalive(client_fd);
    socket_helper::nonblocking(client_fd);

    // create a new socket object
    socket* new_socket_ptr = new_socket(socket_id, client_fd, protocol_type::TCP, socket_ptr->svc_handle, false);
    if (new_socket_ptr == nullptr)
    {
        ::close(client_fd);
        return 0;
    }

    // recv statistics
    socket_ptr->stat_recv(1, time_);

    // accept new connection
    new_socket_ptr->status = socket::status::PACCEPT;
    result->svc_handle = socket_ptr->svc_handle;
    result->socket_id = socket_ptr->socket_id;
    result->ud = socket_id;
    result->data = nullptr;

    if (to_endpoint(&sa, addr_tmp_buf_, ADDR_TMP_BUFFER_SIZE))
    {
        result->data = addr_tmp_buf_;
    }

    return 1;
}


int socket_server::report_connect(socket* socket_ptr, socket_message* result)
{
    // check socket error
    int error = 0;
    socklen_t len = sizeof(error);
    int code = ::getsockopt(socket_ptr->socket_fd, SOL_SOCKET, SO_ERROR, &error, &len);
    if (code < 0 || error)
    {
        socket_lock sl(socket_ptr->dw_mutex);
        force_close(socket_ptr, sl, result);

        result->data = code >= 0 ? ::strerror(error) : ::strerror(errno);
        
        return socket_event::SOCKET_ERROR;
    }

    // 
    socket_ptr->status = socket::status::CONNECTED;
    result->svc_handle = socket_ptr->svc_handle;
    result->socket_id = socket_ptr->socket_id;
    result->ud = 0;
    if (socket_ptr->nomore_sending_data())
    {
        event_poller_.write(socket_ptr->socket_fd, socket_ptr, false);
    }
    socket_addr sa;
    socklen_t sa_sz = sizeof(sa);
    if (::getpeername(socket_ptr->socket_fd, &sa.s, &sa_sz) == 0)
    {
        void* sin_addr = (sa.s.sa_family == AF_INET) ? (void*)&sa.v4.sin_addr : (void*)&sa.v6.sin6_addr;
        if (::inet_ntop(sa.s.sa_family, sin_addr, addr_tmp_buf_, ADDR_TMP_BUFFER_SIZE))
        {
            result->data = addr_tmp_buf_;
            return socket_event::SOCKET_OPEN;
        }
    }
    result->data = nullptr;

    return socket_event::SOCKET_OPEN;
}

// 单个socket每次从内核尝试读取的数据字节数为sz
// 比如，客户端发了一个1kb的数据，socket线程会从内核里依次读取64b，128b，256b，512b，64b数据，总共需读取5次，即会向gateserver服务发5条消息，一个TCP包被切割成5个数据块。
// 第5次尝试读取1024b数据，所以可能会读到其他TCP包的数据(只要客户端有发送其他数据)。接下来，客户端再发一个1kb的数据，socket线程只需从内核读取一次即可。
// return -1 (ignore) when error
int socket_server::forward_message_tcp(socket* socket_ptr, socket_lock& sl, socket_message* result)
{
    int sz = socket_ptr->p.size;
    char* buffer = new char[sz]{0};
    // 单个socket每次从内核尝试读取的数据字节数为sz, 这个值保存在s->p.size中，
    // 初始是MIN_READ_BUFFER(64b)，当实际读到的数据等于sz时，sz扩大一倍(8-9行);
    int n = (int)::read(socket_ptr->socket_fd, buffer, sz);	
    if (n < 0)
    {
        delete[] buffer;

        if (errno == EINTR)
            return -1;
        if (errno == AGAIN_WOULDBLOCK)
        {
            std::cerr << "socket-server : EAGAIN capture." << std::endl;
            return -1;
        }

        // close when error
        force_close(socket_ptr, sl, result);
        result->data = ::strerror(errno);

        return socket_event::SOCKET_ERROR;
    }
    //
    if (n == 0)
    {
        delete[] buffer;

        force_close(socket_ptr, sl, result);
        return socket_event::SOCKET_CLOSE;
    }

    if (socket_ptr->status == socket::status::HALF_CLOSE)
    {
        // discard recv data
        delete[] buffer;
        return -1;
    }

    socket_ptr->stat_recv(n, time_);

    if (n == sz)
    {
        socket_ptr->p.size *= 2;
    }
    // 如果小于sz的一半，则设置sz为原来的一半
    else if (sz > MIN_READ_BUFFER && n * 2 < sz)
    {
        socket_ptr->p.size /= 2;
    }

    result->svc_handle = socket_ptr->svc_handle;
    result->socket_id = socket_ptr->socket_id;
    result->ud = n;
    result->data = buffer;

    return socket_event::SOCKET_DATA;
}

int socket_server::forward_message_udp(socket* socket_ptr, socket_lock& sl, socket_message* result)
{
    socket_addr sa;
    socklen_t sa_sz = sizeof(sa);
    int n = ::recvfrom(socket_ptr->socket_fd, udp_recv_buf_, MAX_UDP_PACKAGE, 0, &sa.s, &sa_sz);
    if (n < 0)
    {
        if (errno == EINTR || errno == AGAIN_WOULDBLOCK)
            return -1;
        
        // close when error
        force_close(socket_ptr, sl, result);
        result->data = ::strerror(errno);

        return socket_event::SOCKET_ERROR;
    }

    // recv statistics
    socket_ptr->stat_recv(n, time_);

    // 将udp地址信息附加到数据尾部
    uint8_t* data_ptr = nullptr;
    // udp v4
    if (sa_sz == sizeof(sa.v4))
    {
        // protocol type must udp v4
        if (socket_ptr->protocol != protocol_type::UDP)
            return -1;

        data_ptr = new uint8_t[n + 1 + 2 + 4]{0};
        socket_addr_to_udp_address(protocol_type::UDP, &sa, data_ptr + n);
    }
    // udp v6
    else
    {
        // protocol type must udp v6
        if (socket_ptr->protocol != protocol_type::UDPv6)
            return -1;

        data_ptr = new uint8_t[n + 1 + 2 + 16]{0};
        socket_addr_to_udp_address(protocol_type::UDPv6, &sa, data_ptr + n);
    }
    ::memcpy(data_ptr, udp_recv_buf_, n);

    result->svc_handle = socket_ptr->svc_handle;
    result->socket_id = socket_ptr->socket_id;
    result->ud = n;
    result->data = (char*)data_ptr;

    return socket_event::SOCKET_UDP;
}

bool socket_server::send_object_init(send_object* so, const void* object, size_t sz)
{
    if (sz == USER_OBJECT)
    {
        so->buffer = soi_.buffer(object);
        so->sz = soi_.size(object);
//         so->free_func = soi_.free;
        return true;
    }
    else
    {
        so->buffer = object;
        so->sz = sz;
//         so->free_func = skynet_free;
        return false;
    }
}

void socket_server::send_object_init(send_object* so, send_buffer* buf_ptr)
{
    if (buf_ptr == nullptr)
        return;

    if (buf_ptr->type == buffer_type::MEMORY)
    {
        send_object_init(so, (void*)buf_ptr->buffer, buf_ptr->sz);
    }
    else if (buf_ptr->type == buffer_type::OBJECT)
    {
        send_object_init(so, (void*)buf_ptr->buffer, USER_OBJECT);
    }
    else if (buf_ptr->type == buffer_type::RAW_POINTER)
    {
        so->buffer = (void*)buf_ptr->buffer;
        so->sz = buf_ptr->sz;
        so->free_func = [](void* ptr) { (void)ptr; };
    }
    // never get here
    else
    {
        so->buffer = nullptr;
        so->sz = 0;
        so->free_func = nullptr;
    }
}


bool socket_server::_query_socket_info(socket& socket_ref, socket_info& si)
{
    socket_addr sa;
    socklen_t sa_sz = sizeof(sa);

    uint8_t status = socket_ref.status;
    switch (status)
    {
    case socket::status::BIND:
        {
            si.status = socket_info::status::BIND;
            si.endpoint[0] = '\0';
        }
        break;
    case socket::status::LISTEN:
        {
            si.status = socket_info::status::LISTEN;
            // local server listen address
            if (::getsockname(socket_ref.socket_fd, &sa.s, &sa_sz) == 0)
                to_endpoint(&sa, si.endpoint, sizeof(si.endpoint));
        }
        break;
    case socket::status::CONNECTED:
        if (socket_ref.protocol == protocol_type::TCP)
        {
            si.status = socket_info::status::TCP;
            // remote client address
            if (::getpeername(socket_ref.socket_fd, &sa.s, &sa_sz) == 0)
                to_endpoint(&sa, si.endpoint, sizeof(si.endpoint));
        }
        else
        {
            si.status = socket_info::status::UDP;
            // 
            if (udp_address_to_socket_addr(socket_ref.protocol, socket_ref.p.udp_address, sa))
                to_endpoint(&sa, si.endpoint, sizeof(si.endpoint));
        }
        break;
    default:
        return false;
    }

    // base info
    si.socket_id = socket_ref.socket_id;
    si.svc_handle = socket_ref.svc_handle;
    
    // send/recv statistics info
    si.recv = socket_ref.stat.recv;
    si.send = socket_ref.stat.send;
    si.recv_time = socket_ref.stat.recv_time;
    si.send_time = socket_ref.stat.send_time;
    
    // write buffer size
    si.wb_size = socket_ref.wb_size;

    return true;
}

} }

