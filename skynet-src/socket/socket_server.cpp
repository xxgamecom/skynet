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
#include "socket_helper.h"

#include "../log/log.h"

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

namespace skynet {

enum
{
    MIN_READ_BUFFER                 = 64,
    
    WARNING_SIZE                    = 1024 * 1024,

    SIZEOF_TCP_BUFFER               = offsetof(write_buffer, udp_address[0]),
    SIZEOF_UDP_BUFFER               = sizeof(write_buffer),
};

// buffer priority
enum priority_type
{
    PRIORITY_TYPE_HIGH              = 0,
    PRIORITY_TYPE_LOW               = 1,
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
        log(nullptr, "socket-server: create event poll fd failed.");
        return false;
    }

    //    
    if (!pipe_.init())
    {
        log(nullptr, "socket-server: create pipe failed.");
        return false;
    }

    // add read ctrl fd to event poll
    int pipe_read_fd = pipe_.read_fd();
    if (!event_poller_.add(pipe_read_fd, nullptr))
    {
        log(nullptr, "socket-server: can't add server socket fd to event poll.");
        
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
    std::array<socket, socket_pool::MAX_SOCKET>& all_sockets = socket_pool_.get_all_sockets();
    for (auto& socket_ref : all_sockets)
    {
        if (socket_ref.status != SOCKET_STATUS_ALLOCED)
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

// @return -1 failed, socket fd
//         family: AF_INET, AF_INET6
static int do_bind(const char* host, int port, int protocol_type, int* family)
{
    struct addrinfo ai_hints;
    memset(&ai_hints, 0, sizeof(ai_hints));
    ai_hints.ai_family = AF_UNSPEC;
    // tcp
    if (protocol_type == IPPROTO_TCP)
    {
        ai_hints.ai_socktype = SOCK_STREAM;
    }
    // udp
    else
    {
        assert(protocol_type == IPPROTO_UDP);
        ai_hints.ai_socktype = SOCK_DGRAM;
    }
    ai_hints.ai_protocol = protocol_type;

    // host: INADDR_ANY
    if (host == nullptr || host[0] == 0)
        host = "0.0.0.0";
    // port
    char portstr[16] = { 0 };
    ::sprintf(portstr, "%d", port);

    struct addrinfo* ai_list = nullptr;
    int status = ::getaddrinfo(host, portstr, &ai_hints, &ai_list);
    if (status != 0)
    {
        return -1;
    }

    *family = ai_list->ai_family;
    int fd = ::socket(*family, ai_list->ai_socktype, 0);
    if (fd < 0)
    {
        ::freeaddrinfo(ai_list);
        return -1;
    }

    // reuse address
    if (!socket_helper::reuse_address(fd))
    {
        ::close(fd);
        ::freeaddrinfo(ai_list);
        return -1;
    }

    // socket binding
    status = ::bind(fd, (struct sockaddr *)ai_list->ai_addr, ai_list->ai_addrlen);
    if (status != 0)
    {
        ::close(fd);
        ::freeaddrinfo(ai_list);
        return -1;
    }

    //
    ::freeaddrinfo(ai_list);
    return fd;
}

static int do_listen(const char* host, int port, int backlog)
{
    // bind
    int family = 0;
    int listen_fd = do_bind(host, port, IPPROTO_TCP, &family);
    if (listen_fd < 0)
    {
        return -1;
    }

    // listen
    if (::listen(listen_fd, backlog) == -1)
    {
        ::close(listen_fd);
        return -1;
    }

    return listen_fd;
}

//
int socket_server::listen(uint64_t svc_handle, const char* addr, int port, int backlog)
{
    // 调用unix系统接口bind，listen获取一个fd
    int listen_fd = do_listen(addr, port, backlog);
    if (listen_fd < 0)
        return -1;

    // 从ss的socket池中获取空闲的socket, 并返回id
    int socket_id = socket_pool_.alloc_socket_id();
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

int socket_server::connect(uint64_t svc_handle, const char* addr, int port)
{
    // 分配一个socket
    int socket_id = socket_pool_.alloc_socket_id();
    if (socket_id < 0)
        return -1;

    ctrl_cmd_package cmd;
    int len = prepare_ctrl_cmd_request_open(cmd, svc_handle, socket_id, addr, port);
    if (len < 0)
        return -1;

    _send_ctrl_cmd(&cmd);

    return socket_id;
}

void socket_server::start(uint64_t svc_handle, int socket_id)
{
    ctrl_cmd_package cmd;
    prepare_ctrl_cmd_request_resume(cmd, svc_handle, socket_id);
    _send_ctrl_cmd(&cmd);
}

void socket_server::pause(uint64_t svc_handle, int socket_id)
{
    ctrl_cmd_package cmd;
    prepare_ctrl_cmd_request_pause(cmd, svc_handle, socket_id);
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
                if (type == SOCKET_EVENT_CLOSE || type == SOCKET_EVENT_ERROR)
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
        auto socket_ptr = event_ref.socket_ptr;
        if (socket_ptr == nullptr)
            continue;

        socket_lock sl(socket_ptr->dw_mutex);

        // 根据socket状态做相应的处理
        switch (socket_ptr->status)
        {
        // socket正在连接
        case SOCKET_STATUS_CONNECTING:
            return handle_connect(socket_ptr, sl, result);
        //
        case SOCKET_STATUS_LISTEN:
            {
                int ok = handle_accept(socket_ptr, result);
                if (ok > 0)
                    return SOCKET_EVENT_ACCEPT;
                if (ok < 0)
                    return SOCKET_EVENT_ERROR;
                
                // ok == 0, retry
                break;
            }
        case SOCKET_STATUS_INVALID:
            log(nullptr, "socket-server : invalid socket");
            break;
        default:
            // 如果socket已连接且事件可读，通过forward_message_tcp接收数据
            if (event_ref.is_readable)
            {
                int socket_event;
                if (socket_ptr->protocol_type == SOCKET_TYPE_TCP)
                {
                    socket_event = forward_message_tcp(socket_ptr, sl, result);
                }
                else
                {
                    socket_event = forward_message_udp(socket_ptr, sl, result);

                    // 尝试再次读取
                    if (socket_event == SOCKET_EVENT_UDP)
                    {
                        --event_next_index_;
                        return SOCKET_EVENT_UDP;
                    }
                }

                // Try to dispatch write message next step if write flag set.
                if (event_ref.is_writeable &&
                    socket_event != SOCKET_EVENT_CLOSE &&
                    socket_event != SOCKET_EVENT_ERROR)
                {
                    event_ref.is_readable = false;
                    --event_next_index_;
                }

                //
                if (socket_event == -1)
                    break;
                
                return socket_event;
            }

            // 如果socket已连接且事件可写，通过send_buffer发送数据。
            if (event_ref.is_writeable)
            {
                int socket_event = send_write_buffer(socket_ptr, sl, result);
                
                // blocked, 稍后再发
                if (socket_event == -1)
                    break;
                
                return socket_event;
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
                result->data_ptr = const_cast<char*>(err);

                return SOCKET_EVENT_ERROR;
            }
            
            // eof
            if (event_ref.is_eof)
            {
                // For epoll (at least), FIN packets are exchanged both ways.
                // See: https://stackoverflow.com/questions/52976152/tcp-when-is-epollhup-generated
                force_close(socket_ptr, sl, result);
                if (socket_ptr->is_close_read())
                {
                    // already raised SOCKET_EVENT_CLOSE
                    return -1;
                }
                else
                {
                    return SOCKET_EVENT_CLOSE;
                }
            }

            break;
        }
    }
}

int socket_server::send(send_buffer* buf)
{
    int socket_id = buf->socket_id;
    auto& socket_ref = socket_pool_.get_socket(socket_id);

    if (socket_ref.is_invalid(socket_id))
    {
        free_send_buffer(buf);
        return -1;
    }

    // 是否可以立刻发送数据，当该socket的发送队列缓冲区为空，且立刻写的缓冲区也为空时，可直接发送。

    // scope lock
    std::unique_lock <std::mutex> sl(socket_ref.dw_mutex);

    if (socket_ref.can_direct_write(socket_id) && sl.try_lock())
    {
        // may be we can send directly, double check
        if (socket_ref.can_direct_write(socket_id))
        {
            // 直接发送
            send_object so;
            send_object_init(&so, buf);
            ssize_t n = 0;

            // tcp
            if (socket_ref.protocol_type == SOCKET_TYPE_TCP)
            {
                n = ::write(socket_ref.socket_fd, so.buffer, so.sz);
            }
            // udp
            else
            {
                socket_addr sa;
                int sa_sz = sa.from_udp_address(socket_ref.protocol_type, socket_ref.p.udp_address);
                if (sa_sz == 0)
                {
                    log(nullptr, "socket-server : set udp (%d) address first.", socket_id);

                    so.free_func((void*)buf->data_ptr);
                    return -1;
                }
                n = ::sendto(socket_ref.socket_fd, so.buffer, so.sz, 0, &sa.addr.s, sa_sz);
            }

            // error
            if (n < 0)
                n = 0; // ignore error, let socket thread try again

            // 发统计
            socket_ref.stat_send(n, time_);

            // 写完
            if (n == so.sz)
            {
                so.free_func((void*)buf->data_ptr);
                return 0;
            }

            // 直接发送失败, 将buffer加入到s->dw_*, 让socket线程去发送. 参考: send_write_buffer()
            socket_ref.dw_buffer = clone_send_buffer(buf, &socket_ref.dw_size);
            socket_ref.dw_offset = n;

            //
            sl.unlock();

            socket_ref.inc_sending_ref(socket_id);

            // let socket thread enable write event
            ctrl_cmd_package cmd;
            prepare_ctrl_cmd_request_trigger_write(cmd, socket_id);
            _send_ctrl_cmd(&cmd);

            return 0;
        }

        sl.unlock();
    }

    //
    socket_ref.inc_sending_ref(socket_id);

    //
    ctrl_cmd_package cmd;
    const send_buffer* clone_buf_ptr = (const send_buffer*)clone_send_buffer(buf, &cmd.u.send.data_size);
    prepare_ctrl_cmd_request_send(cmd, socket_id, clone_buf_ptr, true);
    _send_ctrl_cmd(&cmd);

    return 0;
}

int socket_server::send_low_priority(send_buffer* buf)
{
    int socket_id = buf->socket_id;
    auto& socket_ref = socket_pool_.get_socket(socket_id);

    if (socket_ref.is_invalid(socket_id))
    {
        free_send_buffer(buf);
        return -1;
    }

    // 增加发送计数
    socket_ref.inc_sending_ref(socket_id);

    // 
    ctrl_cmd_package cmd;
    const send_buffer* clone_buf_ptr = (const send_buffer*)clone_send_buffer(buf, &cmd.u.send.data_size);
    prepare_ctrl_cmd_request_send(cmd, socket_id, clone_buf_ptr, false);
    _send_ctrl_cmd(&cmd);

    return 0;
}

int socket_server::bind(uint64_t svc_handle, int fd)
{
    // 分配一个socket
    int socket_id = socket_pool_.alloc_socket_id();
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

int socket_server::udp(uint64_t svc_handle, const char* addr, int port)
{
    int fd = INVALID_FD;
    int family;
    if (port != 0 || addr != NULL)
    {
        // bind
        fd = do_bind(addr, port, IPPROTO_UDP, &family);
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

    //
    socket_helper::nonblocking(fd);

    int socket_id = socket_pool_.alloc_socket_id();
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
    auto& socket_ref = socket_pool_.get_socket(socket_id);
    if (socket_ref.is_invalid(socket_id))
    {
        free_send_buffer(buf);
        return -1;
    }

    const uint8_t* udp_address = (const uint8_t*)addr;
    int addr_sz;
    switch (udp_address[0])
    {
    case SOCKET_TYPE_UDP:
        addr_sz = 1 + 2 + 4;    // 1 type, 2 port, 4 ipv4
        break;
    case SOCKET_TYPE_UDPv6:
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
                socklen_t sa_sz = sa.from_udp_address(socket_ref.protocol_type, udp_address);
                if (sa_sz == 0)
                {
                    so.free_func((void*)buf->data_ptr);
                    return -1;
                }

                int send_n = ::sendto(socket_ref.socket_fd, so.buffer, so.sz, 0, &sa.addr.s, sa_sz);
                if (send_n >= 0)
                {
                    // sendto succ
                    socket_ref.stat_send(send_n, time_);
                    so.free_func((void*)buf->data_ptr);
                    return 0;
                }
            }
            // else: let socket thread try again, udp doesn't care the order
        }
    }

    ctrl_cmd_package cmd;
    send_buffer* clone_buf = (send_buffer*)clone_send_buffer(buf, &cmd.u.send_udp.send.data_size);
    prepare_ctrl_cmd_request_send_udp(cmd, socket_id, clone_buf, udp_address, addr_sz);
    _send_ctrl_cmd(&cmd);

    return 0;
}

int socket_server::udp_connect(int socket_id, const char* addr, int port)
{
    auto& socket_ref = socket_pool_.get_socket(socket_id);
    if (socket_ref.is_invalid(socket_id))
        return -1;

    // increase udp_connecting, use scope lock
    {
        std::lock_guard<std::mutex> lock(socket_ref.dw_mutex);

        if (socket_ref.is_invalid(socket_id))
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

    int type;
    if (ai_list->ai_family == AF_INET)
    {
        type = SOCKET_TYPE_UDP;
    }
    else if (ai_list->ai_family == AF_INET6)
    {
        type = SOCKET_TYPE_UDPv6;
    }
    else
    {
        ::freeaddrinfo(ai_list);
        return -1;
    }

    ctrl_cmd_package cmd;
    prepare_ctrl_cmd_request_set_udp(cmd, socket_id, type, (socket_addr*)ai_list->ai_addr);
    ::freeaddrinfo(ai_list);
    _send_ctrl_cmd(&cmd);

    return 0;
}

const socket_udp_address* socket_server::udp_address(socket_message* msg, int* addr_sz)
{
    // type
    uint8_t* address = (uint8_t*)(msg->data_ptr + msg->ud);
    int type = address[0];

    // 必须为udp
    if (type == SOCKET_TYPE_UDP)
        *addr_sz = 1 + 2 + 4;
    else if (type == SOCKET_TYPE_UDPv6)
        *addr_sz = 1 + 2 + 16;
    else
        return nullptr;

    return (const socket_udp_address*)address;
}

//----------------------------------------------
// ctrl cmd
//----------------------------------------------

void socket_server::_send_ctrl_cmd(ctrl_cmd_package* cmd)
{
    // header[6] - type, 1 byte
    // header[7] - data len, 1 byte
    int data_len = cmd->header[7];

    for (;;)
    {
        int n = pipe_.write((const char*)&cmd->header[6], data_len + 2);

        // write failed, retry
        if (n < 0)
        {
            if (errno != EINTR)
            {
                log(nullptr, "socket-server : send ctrl command error %s.", ::strerror(errno));
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
    // recv header: ctrl_cmd (1 byte) + data len (1 byte)
    uint8_t header[2] = { 0 };
    if (pipe_.read((char*)header, 2) == -1)
    {
        log(nullptr, "socket-server : read header from pipe error %s.", ::strerror(errno));
        return -1;
    }

    //
    int ctrl_cmd = header[0];
    int len = header[1];

    // recv data
    uint8_t buf[256] = { 0 };
    if (pipe_.read((char*)buf, len) == -1)
    {
        log(nullptr, "socket-server : read data from pipe error %s.", ::strerror(errno));
        return -1;
    }

    // handle
    switch (ctrl_cmd)
    {
    case 'R':
        return handle_ctrl_cmd_resume_socket((request_resume_pause*)buf, result);
    case 'S':
        return handle_ctrl_cmd_pause_socket((request_resume_pause*)buf, result);
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
    case 'W':
        return handle_ctrl_cmd_trigger_write((request_send*)buf, result);
    case 'D':
    case 'P': 
    {
        int priority = (ctrl_cmd == 'D') ? PRIORITY_TYPE_HIGH : PRIORITY_TYPE_LOW;
        auto cmd = (request_send*)buf;
        int ret = handle_ctrl_cmd_send_socket(cmd, result, priority, nullptr);
        
        auto& socket_ref = socket_pool_.get_socket(cmd->socket_id);
        socket_ref.dec_sending_ref(cmd->socket_id);
        
        return ret;
    }
    case 'A': 
    {
        auto cmd = (request_send_udp*)buf;
        return handle_ctrl_cmd_send_socket(&cmd->send, result, PRIORITY_TYPE_HIGH, cmd->address);
    }
    case 'C':
        return handle_ctrl_cmd_set_udp_address((request_set_udp*)buf, result);
    case 'T':
        return handle_ctrl_cmd_setopt_socket((request_set_opt*)buf);
    case 'U':
        return handle_ctrl_cmd_add_udp_socket((request_udp*)buf);
    }

    //
    log(nullptr, "socket-server : Unknown ctrl command %c.", ctrl_cmd);
    return -1;
}

// return -1 when connecting
int socket_server::handle_ctrl_cmd_open_socket(request_open* cmd, socket_message* result)
{
    int socket_id = cmd->socket_id;

    result->svc_handle = cmd->svc_handle;
    result->socket_id = socket_id;
    result->ud = 0;
    result->data_ptr = nullptr;

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
            result->data_ptr = const_cast<char*>(::gai_strerror(status));
            break;  // failed
        }

        int socket_fd = INVALID_FD;
        addrinfo* ai_ptr = nullptr;
        for (ai_ptr = ai_list; ai_ptr != nullptr; ai_ptr = ai_ptr->ai_next)
        {
            socket_fd = ::socket(ai_ptr->ai_family, ai_ptr->ai_socktype, ai_ptr->ai_protocol);
            if (socket_fd < 0)
                continue;

            socket_helper::keepalive(socket_fd);
            socket_helper::nonblocking(socket_fd);
            status = ::connect(socket_fd, ai_ptr->ai_addr, ai_ptr->ai_addrlen);
            if (status != 0 && errno != EINPROGRESS)
            {
                ::close(socket_fd);
                socket_fd = INVALID_FD;
                continue;
            }
            break;
        }

        // failed
        if (socket_fd < 0)
        {
            result->data_ptr = ::strerror(errno);
            break;
        }

        //
        socket* new_socket_ptr = new_socket(socket_id, socket_fd, SOCKET_TYPE_TCP, cmd->svc_handle);
        if (new_socket_ptr == nullptr)
        {
            ::close(socket_fd);
            result->data_ptr = const_cast<char*>("reach skynet socket number limit");
            break;
        }

        if (status == 0)
        {
            new_socket_ptr->status = SOCKET_STATUS_CONNECTED;
            sockaddr* addr = ai_ptr->ai_addr;
            void* sin_addr = (ai_ptr->ai_family == AF_INET) ? (void*)&((sockaddr_in*)addr)->sin_addr : (void*)&((sockaddr_in6*)addr)->sin6_addr;
            if (::inet_ntop(ai_ptr->ai_family, sin_addr, addr_tmp_buf_, ADDR_TMP_BUFFER_SIZE))
            {
                result->data_ptr = addr_tmp_buf_;
            }
            ::freeaddrinfo(ai_list);
            return SOCKET_EVENT_OPEN;
        }
        else
        {
            new_socket_ptr->status = SOCKET_STATUS_CONNECTING;
            if (enable_write(new_socket_ptr, true))
            {
                result->data_ptr = const_cast<char*>("enable write failed");
                break;
            }
        }

        is_ok = true;
    } while (0);

    // failed
    if (!is_ok)
    {
        ::freeaddrinfo(ai_list);
        socket_pool_.get_socket(socket_id).status = SOCKET_STATUS_INVALID;
        return SOCKET_EVENT_ERROR;
    }

    // success
    ::freeaddrinfo(ai_list);
    return -1;
}

// SOCKET_EVENT_CLOSE can be raised (only once) in one of two conditions.
// See https://github.com/cloudwu/skynet/issues/1346 for more discussion.
// 1. close socket by self, See close_socket()
// 2. recv 0 or eof event (close socket by remote), See forward_message_tcp()
// It's able to write data after SOCKET_EVENT_CLOSE (In condition 2), but if remote is closed, SOCKET_EVENT_ERROR may raised.
int socket_server::handle_ctrl_cmd_close_socket(request_close* cmd, socket_message* result)
{
    int socket_id = cmd->socket_id;
    auto& socket_ref = socket_pool_.get_socket(socket_id);

    // socket is closed, ignore
    if (socket_ref.is_invalid(socket_id))
    {
        return -1;
    }

    socket_lock sl(socket_ref.dw_mutex);

    bool shutdown_read = socket_ref.is_close_read();
    if (cmd->shutdown || socket_ref.nomore_sending_data())
    {
        // -1 or SOCKET_EVENT_WARNING or SOCKET_EVENT_CLOSE,
        //       SOCKET_EVENT_WARNING means nomore_sending_data
        force_close(&socket_ref, sl, result);
        return shutdown_read ? -1 : SOCKET_EVENT_CLOSE;
    }

    //
    socket_ref.closing = true;

    if (!shutdown_read)
    {
        // don't read socket after socket.close()
        close_read(&socket_ref, result);
        return SOCKET_EVENT_CLOSE;
    }

    // recv 0 before (socket is EVENT_HALF_CLOSE_READ) and waiting for sending data out.
    return -1;
}

int socket_server::handle_ctrl_cmd_bind_socket(request_bind* cmd, socket_message* result)
{
    int socket_id = cmd->socket_id;
    result->socket_id = socket_id;
    result->svc_handle = cmd->svc_handle;
    result->ud = 0;

    //
    socket* new_socket_ptr = new_socket(socket_id, cmd->fd, SOCKET_TYPE_TCP, cmd->svc_handle);
    if (new_socket_ptr == nullptr)
    {
        result->data_ptr = const_cast<char*>("reach skynet socket number limit");
        return SOCKET_EVENT_ERROR;
    }

    socket_helper::nonblocking(cmd->fd);
    new_socket_ptr->status = SOCKET_STATUS_BIND;
    result->data_ptr = const_cast<char*>("binding");

    return SOCKET_EVENT_OPEN;
}

int socket_server::handle_ctrl_cmd_resume_socket(request_resume_pause* cmd, socket_message* result)
{
    int socket_id = cmd->socket_id;

    //
    result->socket_id = socket_id;
    result->svc_handle = cmd->svc_handle;
    result->ud = 0;
    result->data_ptr = nullptr;

    auto& socket_ref = socket_pool_.get_socket(socket_id);
    if (socket_ref.is_invalid(socket_id))
    {
        result->data_ptr = const_cast<char*>("invalid socket");
        return SOCKET_EVENT_ERROR;
    }

    if (socket_ref.is_close_read())
    {
        return -1;
    }

    if (enable_read(&socket_ref, true))
    {
        result->data_ptr = const_cast<char*>("enable read failed");
        return SOCKET_EVENT_ERROR;
    }

    //
    if (socket_ref.status == SOCKET_STATUS_PREPARE_ACCEPT || socket_ref.status == SOCKET_STATUS_PREPARE_LISTEN)
    {
        if (socket_ref.status == SOCKET_STATUS_PREPARE_ACCEPT)
            socket_ref.status = SOCKET_STATUS_CONNECTED;
        else
            socket_ref.status = SOCKET_STATUS_LISTEN;
        socket_ref.svc_handle = cmd->svc_handle;
        result->data_ptr = const_cast<char*>("start");

        return SOCKET_EVENT_OPEN;
    }
    //
    else if (socket_ref.status == SOCKET_STATUS_CONNECTED)
    {
        // todo: maybe we should send a message SOCKET_TRANSFER to socket_ptr->svc_handle
        socket_ref.svc_handle = cmd->svc_handle;
        result->data_ptr = const_cast<char*>("transfer");
        return SOCKET_EVENT_OPEN;
    }

    // if socket_ptr->status == SOCKET_STATUS_HALF_CLOSE_* , SOCKET_STATUS_SOCKET_CLOSE message will send later
    return -1;
}

int socket_server::handle_ctrl_cmd_pause_socket(request_resume_pause* cmd, socket_message* result)
{
    int socket_id = cmd->socket_id;

    auto& socket_ref = socket_pool_.get_socket(socket_id);
    if (socket_ref.is_invalid(socket_id))
    {
        return -1;
    }

    if (enable_read(&socket_ref, false))
    {
        result->socket_id = socket_id;
        result->svc_handle = cmd->svc_handle;
        result->ud = 0;
        result->data_ptr = const_cast<char*>("enable read failed");
        return SOCKET_EVENT_ERROR;
    }

    return -1;
}

int socket_server::handle_ctrl_cmd_setopt_socket(request_set_opt* cmd)
{
    int socket_id = cmd->socket_id;

    auto& socket_ref = socket_pool_.get_socket(socket_id);
    if (socket_ref.is_invalid(socket_id))
        return SOCKET_EVENT_ERROR;

    int v = cmd->value;
    ::setsockopt(socket_ref.socket_fd, IPPROTO_TCP, cmd->what, &v, sizeof(v));

    return -1;
}

int socket_server::handle_ctrl_cmd_exit_socket(socket_message* result)
{
    result->svc_handle = 0;
    result->socket_id = 0;
    result->ud = 0;
    result->data_ptr = nullptr;

    return SOCKET_EVENT_EXIT;
}

/**
 * send data, can set data send priority: PRIORITY_TYPE_HIGH | PRIORITY_TYPE_LOW
 * 
 * 1) if socket send buffer is empty, write data to socket fd directly.
 * 2) if part of data is written, write the rest to the high priority list. (even the priority is PRIORITY_TYPE_LOW)
 * 3) otherwish, 将数据添加到高优先级队列(PRIORITY_TYPE_HIGH) 或 低优先级队列(PRIORITY_TYPE_LOW).
 */
int socket_server::handle_ctrl_cmd_send_socket(request_send* cmd, socket_message* result, int priority, const uint8_t* udp_address)
{
    int socket_id = cmd->socket_id;
    auto& socket_ref = socket_pool_.get_socket(socket_id);

    send_object so;
    send_object_init(&so, cmd->data_ptr, cmd->data_size);

    // can't send data when the socket is 'invalid' or 'write closed' or '' or 'closing'
    if (socket_ref.is_invalid(socket_id) ||
        socket_ref.is_close_write() ||
        socket_ref.status == SOCKET_STATUS_PREPARE_ACCEPT ||
        socket_ref.closing)
    {
        so.free_func((void*)cmd->data_ptr);
        return -1;
    }

    // can't send data to a listen fd
    if (socket_ref.status == SOCKET_STATUS_PREPARE_LISTEN || socket_ref.status == SOCKET_STATUS_LISTEN)
    {
        log(nullptr, "socket-server : write to listen %d.", socket_id);
        so.free_func((void*)cmd->data_ptr);
        return -1;
    }

    // socket send buffer is empty, write data to socket fd directly.
    if (socket_ref.is_send_buffer_empty())
    {
        // tcp
        if (socket_ref.protocol_type == SOCKET_TYPE_TCP)
        {
            // add to high priority list, even priority == PRIORITY_TYPE_LOW
            append_sendbuffer(&socket_ref, cmd);
        }
        // udp
        else
        {
            if (udp_address == nullptr)
            {
                udp_address = socket_ref.p.udp_address;
            }

            socket_addr sa;
            socklen_t sa_sz = sa.from_udp_address(socket_ref.protocol_type, udp_address);
            if (sa_sz == 0)
            {
                // udp type mismatch, just drop it.
                log(nullptr, "socket-server : udp (%d) type mismatch.", socket_id);

                so.free_func((void*)cmd->data_ptr);
                return -1;
            }

            //
            int n = ::sendto(socket_ref.socket_fd, so.buffer, so.sz, 0, &sa.addr.s, sa_sz);
            if (n != so.sz)
            {
                append_sendbuffer(&socket_ref, cmd, priority == PRIORITY_TYPE_HIGH, udp_address);
            }
            else
            {
                // statistics
                socket_ref.stat_send(n, time_);

                //
                so.free_func((void*)cmd->data_ptr);
                return -1;
            }
        }

        //
        if (enable_write(&socket_ref, true))
        {
            result->svc_handle = socket_ref.svc_handle;
            result->socket_id = socket_ref.socket_id;
            result->ud = 0;
            result->data_ptr = const_cast<char*>("enable write failed");

            return SOCKET_EVENT_ERROR;
        }
    }
    //
    else
    {
        // tcp
        if (socket_ref.protocol_type == SOCKET_TYPE_TCP)
        {
            append_sendbuffer(&socket_ref, cmd, priority == PRIORITY_TYPE_HIGH);
        }
        // udp
        else
        {
            if (udp_address == nullptr)
            {
                udp_address = socket_ref.p.udp_address;
            }

            append_sendbuffer(&socket_ref, cmd, priority == PRIORITY_TYPE_HIGH, udp_address);
        }
    }

    // check write size, warning
    if (socket_ref.wb_size >= WARNING_SIZE && socket_ref.wb_size >= socket_ref.warn_size)
    {
        socket_ref.warn_size = socket_ref.warn_size == 0 ? WARNING_SIZE * 2 : socket_ref.warn_size * 2;
        result->svc_handle = socket_ref.svc_handle;
        result->socket_id = socket_ref.socket_id;
        result->ud = socket_ref.wb_size % 1024 == 0 ? socket_ref.wb_size / 1024 : socket_ref.wb_size / 1024 + 1;
        result->data_ptr = nullptr;

        return SOCKET_EVENT_WARNING;
    }
    
    return -1;
}

int socket_server::handle_ctrl_cmd_trigger_write(request_send* cmd, socket_message* result)
{
    int socket_id = cmd->socket_id;
    auto& socket_ref = socket_pool_.get_socket(socket_id);

    //
    if (socket_ref.is_invalid(socket_id))
        return -1;

    if (enable_write(&socket_ref, true))
    {
        result->svc_handle = socket_ref.svc_handle;
        result->socket_id = socket_ref.socket_id;
        result->ud = 0;
        result->data_ptr = const_cast<char*>("enable write failed");

        return SOCKET_EVENT_ERROR;
    }

    return -1;
}

int socket_server::handle_ctrl_cmd_listen_socket(request_listen* cmd, socket_message* result)
{
    int socket_id = cmd->socket_id;
    int listen_fd = cmd->fd;

    // 
    socket* new_socket_ptr = new_socket(socket_id, listen_fd, SOCKET_TYPE_TCP, cmd->svc_handle, false);
    if (new_socket_ptr == nullptr)
    {
        ::close(listen_fd);

        result->svc_handle = cmd->svc_handle;
        result->socket_id = socket_id;
        result->ud = 0;
        result->data_ptr = const_cast<char*>("reach socket number limit");

        //
        socket_pool_.get_socket(socket_id).status = SOCKET_STATUS_INVALID;

        return SOCKET_EVENT_ERROR;
    }
    
    new_socket_ptr->status = SOCKET_STATUS_PREPARE_LISTEN;

    return -1;
}

int socket_server::handle_ctrl_cmd_add_udp_socket(request_udp* cmd)
{
    int socket_id = cmd->socket_id;
    int type = cmd->family == AF_INET6 ? SOCKET_TYPE_UDPv6 : SOCKET_TYPE_UDP;

    socket* new_socket_ptr = new_socket(socket_id, cmd->fd, type, cmd->svc_handle);
    if (new_socket_ptr == nullptr)
    {
        ::close(cmd->fd);
        socket_pool_.get_socket(socket_id).status = SOCKET_STATUS_INVALID;
        return SOCKET_EVENT_ERROR;
    }

    new_socket_ptr->status = SOCKET_STATUS_CONNECTED;
    ::memset(new_socket_ptr->p.udp_address, 0, sizeof(new_socket_ptr->p.udp_address));

    return -1;
}

int socket_server::handle_ctrl_cmd_set_udp_address(request_set_udp* cmd, socket_message* result)
{
    int socket_id = cmd->socket_id;
    auto& socket_ref = socket_pool_.get_socket(socket_id);

    if (socket_ref.is_invalid(socket_id))
        return -1;

    //
    int type = cmd->address[0];
    if (type != socket_ref.protocol_type)
    {
        // type mismatch
        result->svc_handle = socket_ref.svc_handle;
        result->socket_id = socket_ref.socket_id;
        result->ud = 0;
        result->data_ptr = const_cast<char*>("socket type mismatch");

        return SOCKET_EVENT_ERROR;
    }

    if (type == SOCKET_TYPE_UDP)
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
        auto socket_ptr = event_ref.socket_ptr;

        if (socket_ptr == nullptr)
            continue;

        if (socket_ptr->is_invalid(socket_id))
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
    result->data_ptr = nullptr;

    //
    if (socket_ptr->status == SOCKET_STATUS_INVALID)
        return;

    assert(socket_ptr->status != SOCKET_STATUS_ALLOCED);
    free_write_buffer_list(&socket_ptr->wb_list_high);
    free_write_buffer_list(&socket_ptr->wb_list_low);

    event_poller_.del(socket_ptr->socket_fd);
    
    sl.lock();
    if (socket_ptr->status != SOCKET_STATUS_BIND)
    {
        if (::close(socket_ptr->socket_fd) < 0)
            perror("close socket:");
    }
    socket_ptr->status = SOCKET_STATUS_INVALID;
    if (socket_ptr->dw_buffer != nullptr)
    {
        send_buffer tmp;
        tmp.data_ptr = socket_ptr->dw_buffer;
        tmp.data_size = socket_ptr->dw_size;
        tmp.socket_id = socket_ptr->socket_id;
        tmp.type = (tmp.data_size == USER_OBJECT) ? BUFFER_TYPE_OBJECT : BUFFER_TYPE_MEMORY;
        free_send_buffer(&tmp);
        socket_ptr->dw_buffer = nullptr;
    }
    sl.unlock();
}

void socket_server::close_read(socket* socket_ptr, socket_message* result)
{
    // Don't read socket later
    socket_ptr->shutdown_read();

    enable_read(socket_ptr, false);

    ::shutdown(socket_ptr->socket_fd, SHUT_RD);

    result->socket_id = socket_ptr->socket_id;
    result->ud = 0;
    result->data_ptr = nullptr;
    result->svc_handle = socket_ptr->svc_handle;
}

int socket_server::close_write(socket* socket_ptr, socket_lock& sl, socket_message* result)
{
    // closing
    if (socket_ptr->closing)
    {
        force_close(socket_ptr, sl, result);
        return SOCKET_EVENT_RST;
    }
    // close reading, recv 0 before, ignore the error and close fd
    if (socket_ptr->is_close_read())
    {
        force_close(socket_ptr, sl, result);
        return SOCKET_EVENT_RST;
    }
    // close writing, already raise SOCKET_EVENT_ERROR
    if (socket_ptr->is_close_write())
    {
        return SOCKET_EVENT_RST;
    }

    // close write
    socket_ptr->shutdown_write();

    ::shutdown(socket_ptr->socket_fd, SHUT_WR);

    enable_write(socket_ptr, false);

    result->socket_id = socket_ptr->socket_id;
    result->ud = 0;
    result->svc_handle = socket_ptr->svc_handle;
    result->data_ptr = ::strerror(errno);

    return SOCKET_EVENT_ERROR;
}

int socket_server::enable_write(socket* socket_ptr, bool enable)
{
    if (socket_ptr->writing != enable)
    {
        socket_ptr->writing = enable;
        return event_poller_.enable(socket_ptr->socket_fd, socket_ptr, socket_ptr->reading, enable);
    }

    return 0;
}

int socket_server::enable_read(socket* socket_ptr, bool enable)
{
    if (socket_ptr->reading != enable)
    {
        socket_ptr->reading = enable;
        return event_poller_.enable(socket_ptr->socket_fd, socket_ptr, enable, socket_ptr->writing);
    }

    return 0;
}

void socket_server::drop_udp(socket* socket_ptr, write_buffer_list* wb_list, write_buffer* wb)
{
    socket_ptr->wb_size -= wb->sz;
    wb_list->head = wb->next;
    if (wb_list->head == nullptr)
        wb_list->tail = nullptr;
    free_write_buffer(wb);
}

socket* socket_server::new_socket(int socket_id, int socket_fd, int protocol_type, uint64_t svc_handle, bool reading/* = true*/)
{
    auto& socket_ref = socket_pool_.get_socket(socket_id);
    assert(socket_ref.status == SOCKET_STATUS_ALLOCED);

    // add to event poller
    if (!event_poller_.add(socket_fd, &socket_ref))
    {
        socket_ref.status = SOCKET_STATUS_INVALID;
        return nullptr;
    }

    // 
    socket_ref.socket_id = socket_id;
    socket_ref.socket_fd = socket_fd;
    socket_ref.reading = true;
    socket_ref.writing = false;
    socket_ref.closing = false;
    socket_ref.sending = socket_pool::socket_id_tag16(socket_id) << 16 | 0; // high 16 bits: socket id, low 16 bits: actually sending count
    socket_ref.protocol_type = protocol_type;
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

    if (enable_read(&socket_ref, reading))
    {
        socket_ref.status = SOCKET_STATUS_INVALID;
        return nullptr;
    }

    return &socket_ref;
}

void socket_server::free_send_buffer(send_buffer* buf_ptr)
{
    if (buf_ptr == nullptr)
        return;

    char* buffer = (char*)buf_ptr->data_ptr;
    if (buf_ptr->type == BUFFER_TYPE_MEMORY)
    {
        delete[] buffer;
    }
    else if (buf_ptr->type == BUFFER_TYPE_OBJECT)
    {
        soi_.free(buffer);
    }
    else if (buf_ptr->type == BUFFER_TYPE_RAW_POINTER)
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
    
    if (buf_ptr->type == BUFFER_TYPE_MEMORY)
    {
        *sz = buf_ptr->data_size;
        return buf_ptr->data_ptr;
    }
    else if (buf_ptr->type == BUFFER_TYPE_OBJECT)
    {
        *sz = USER_OBJECT;
        return buf_ptr->data_ptr;
    }
    // It's a raw pointer, we need make a copy
    else if (buf_ptr->type == BUFFER_TYPE_RAW_POINTER)
    {
        *sz = buf_ptr->data_size;
        void* tmp = new char[*sz];
        ::memcpy(tmp, buf_ptr->data_ptr, *sz);
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
    write_buf_ptr->is_user_object = send_object_init(&so, cmd->data_ptr, cmd->data_size);
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
    if (wb->is_user_object)
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
        write_buf_ptr->is_user_object = send_object_init(&so, (void*)socket_ptr->dw_buffer, socket_ptr->dw_size);
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
    int socket_event = do_send_write_buffer(socket_ptr, sl, result);

    //
    sl.unlock();

    return socket_event;
}


int socket_server::send_write_buffer_list(socket* socket_ptr, write_buffer_list* wb_list, socket_lock& sl, socket_message* result)
{
    if (socket_ptr->protocol_type == SOCKET_TYPE_TCP)
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
                if (errno == EINTR)
                    continue;
                if (errno == AGAIN_WOULDBLOCK)
                    return -1;

                return close_write(socket_ptr, sl, result);
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
        socklen_t sa_sz = sa.from_udp_address(socket_ptr->protocol_type, tmp->udp_address);
        if (sa_sz == 0)
        {
            log(nullptr, "socket-server : udp (%d) type mismatch.", socket_ptr->socket_id);
            drop_udp(socket_ptr, wb_list, tmp);
            return -1;
        }
        
        // 发送数据
        int err = ::sendto(socket_ptr->socket_fd, tmp->ptr, tmp->sz, 0, &sa.addr.s, sa_sz);
        if (err < 0)
        {
            //
            if (errno == EINTR || errno == AGAIN_WOULDBLOCK)
                return -1;

            log(nullptr, "socket-server : udp (%d) sendto error %s.", socket_ptr->socket_id, ::strerror(errno));
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
    int ret = send_write_buffer_list(socket_ptr, &socket_ptr->wb_list_high, sl, result);
    if (ret != -1)
    {
        if (ret == SOCKET_EVENT_ERROR)
        {
            // SOCKET_STATUS_HALF_CLOSE_WRITE
            return SOCKET_EVENT_ERROR;
        }

        // SOCKET_EVENT_RST (ignore)
        return -1;
    }

    //
    if (socket_ptr->wb_list_high.head == nullptr)
    {
        // step 2
        if (socket_ptr->wb_list_low.head != nullptr)
        {
            int ret = send_write_buffer_list(socket_ptr, &socket_ptr->wb_list_low, sl, result);
            if (ret != -1)
            {
                if (ret == SOCKET_EVENT_ERROR)
                {
                    // SOCKET_STATUS_HALF_CLOSE_WRITE
                    return SOCKET_EVENT_ERROR;
                }

                // SOCKET_EVENT_RST (ignore)
                return -1;
            }

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
        if (socket_ptr->closing)
        {
            // finish writing
            force_close(socket_ptr, sl, result);
            return -1;
        }

        int err = enable_write(socket_ptr, false);
        if (err)
        {
            result->svc_handle = socket_ptr->svc_handle;
            result->socket_id = socket_ptr->socket_id;
            result->ud = 0;
            result->data_ptr = const_cast<char*>("disable write failed");
            return SOCKET_EVENT_ERROR;
        }

        if(socket_ptr->warn_size > 0)
        {
            socket_ptr->warn_size = 0;
            result->svc_handle = socket_ptr->svc_handle;
            result->socket_id = socket_ptr->socket_id;
            result->ud = 0;
            result->data_ptr = nullptr;
            return SOCKET_EVENT_WARNING;
        }
    }

    return -1;
}

int socket_server::handle_accept(socket* socket_ptr, socket_message* result)
{
    // wait accept
    socket_addr sa;
    socklen_t sa_sz = sizeof(sa);
    int client_fd = ::accept(socket_ptr->socket_fd, &sa.addr.s, &sa_sz);

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
        result->data_ptr = ::strerror(errno);
        return -1;
    }

    // alloc a new socket id
    int socket_id = socket_pool_.alloc_socket_id();
    if (socket_id < 0)
    {
        ::close(client_fd);
        return 0;
    }

    // set socket option: 'keepalive' & 'nonblocking'
    socket_helper::keepalive(client_fd);
    socket_helper::nonblocking(client_fd);

    // create a new socket object
    socket* new_socket_ptr = new_socket(socket_id, client_fd, SOCKET_TYPE_TCP, socket_ptr->svc_handle, false);
    if (new_socket_ptr == nullptr)
    {
        ::close(client_fd);
        return 0;
    }

    // recv statistics
    socket_ptr->stat_recv(1, time_);

    // accepted
    new_socket_ptr->status = SOCKET_STATUS_PREPARE_ACCEPT;

    //
    result->svc_handle = socket_ptr->svc_handle;
    result->socket_id = socket_ptr->socket_id;
    result->ud = socket_id;
    result->data_ptr = nullptr;

    if (sa.to_string(addr_tmp_buf_, ADDR_TMP_BUFFER_SIZE))
    {
        result->data_ptr = addr_tmp_buf_;
    }

    return 1;
}

int socket_server::handle_connect(socket* socket_ptr, socket_lock& sl, socket_message* result)
{
    // check socket error
    int error = 0;
    socklen_t len = sizeof(error);
    int code = ::getsockopt(socket_ptr->socket_fd, SOL_SOCKET, SO_ERROR, &error, &len);
    if (code < 0 || error)
    {
        force_close(socket_ptr, sl, result);

        result->data_ptr = code >= 0 ? ::strerror(error) : ::strerror(errno);
        
        return SOCKET_EVENT_ERROR;
    }

    // 
    socket_ptr->status = SOCKET_STATUS_CONNECTED;
    result->svc_handle = socket_ptr->svc_handle;
    result->socket_id = socket_ptr->socket_id;
    result->ud = 0;
    if (socket_ptr->nomore_sending_data())
    {
        if (enable_write(socket_ptr, false))
        {
            force_close(socket_ptr, sl, result);

            result->data_ptr = const_cast<char*>("disable write failed");

            return SOCKET_EVENT_ERROR;
        }
    }
    socket_addr sa;
    socklen_t sa_sz = sizeof(sa);
    if (::getpeername(socket_ptr->socket_fd, &sa.addr.s, &sa_sz) == 0)
    {
        void* sin_addr = (sa.addr.s.sa_family == AF_INET) ? (void*)&sa.addr.v4.sin_addr : (void*)&sa.addr.v6.sin6_addr;
        if (::inet_ntop(sa.addr.s.sa_family, sin_addr, addr_tmp_buf_, ADDR_TMP_BUFFER_SIZE))
        {
            result->data_ptr = addr_tmp_buf_;
            return SOCKET_EVENT_OPEN;
        }
    }
    result->data_ptr = nullptr;

    return SOCKET_EVENT_OPEN;
}


struct stream_buffer
{
    char* buf;
    int sz;
};

static char* reserve_buffer(stream_buffer* buffer, int sz)
{
    if (buffer->buf == NULL)
    {
        buffer->buf = (char*)new char[sz];
        return buffer->buf;
    }
    else
    {
        char* newbuffer = (char*)new char[sz + buffer->sz];
        ::memcpy(newbuffer, buffer->buf, buffer->sz);
        char* ret = newbuffer + buffer->sz;
        delete[] buffer->buf;
        buffer->buf = newbuffer;
        return ret;
    }
}

static int read_socket(socket* s, stream_buffer* buffer)
{
    int sz = s->p.size;
    buffer->buf = NULL;
    buffer->sz = 0;
    int rsz = sz;
    for (;;)
    {
        char* buf = reserve_buffer(buffer, rsz);
        int n = (int)::read(s->socket_fd, buf, rsz);
        if (n <= 0)
        {
            if (buffer->sz == 0)
            {
                // read nothing
                delete[] buffer->buf;
                return n;
            }
            else
            {
                // ignore the error or hang up, returns buffer
                // If socket is hang up, SOCKET_EVENT_CLOSE will be send later.
                //    (buffer->sz should be 0 next time)
                break;
            }
        }
        buffer->sz += n;
        if (n < rsz)
        {
            break;
        }
        // n == rsz, read again ( and read more )
        rsz *= 2;
    }
    int r = buffer->sz;
    if (r > sz)
    {
        s->p.size = sz * 2;
    }
    else if (sz > MIN_READ_BUFFER && r * 2 < sz)
    {
        s->p.size = sz / 2;
    }

    return r;
}


// 单个socket每次从内核尝试读取的数据字节数为sz
// 比如，客户端发了一个1kb的数据，socket线程会从内核里依次读取64b，128b，256b，512b，64b数据，总共需读取5次，即会向gateserver服务发5条消息，一个TCP包被切割成5个数据块。
// 第5次尝试读取1024b数据，所以可能会读到其他TCP包的数据(只要客户端有发送其他数据)。接下来，客户端再发一个1kb的数据，socket线程只需从内核读取一次即可。
// return -1 (ignore) when error
int socket_server::forward_message_tcp(socket* socket_ptr, socket_lock& sl, socket_message* result)
{
    stream_buffer buf;
    int n = read_socket(socket_ptr, &buf);
    if (n < 0)
    {
        if (errno == EINTR)
            return -1;
        if (errno == AGAIN_WOULDBLOCK)
        {
            log(nullptr, "socket-server : EAGAIN capture.");
            return -1;
        }

        // close when error
        force_close(socket_ptr, sl, result);
        result->data_ptr = ::strerror(errno);

        return SOCKET_EVENT_ERROR;
    }
    //
    if (n == 0)
    {
        if (socket_ptr->closing)
        {
            // rare case: if socket_ptr->closing is true, reading event is disable, and SOCKET_EVENT_CLOSE is raised.
            if (socket_ptr->nomore_sending_data())
            {
                force_close(socket_ptr, sl, result);
            }
            return -1;
        }

        if (socket_ptr->is_close_read())
        {
            // rare case: already shutdown read.
            return -1;
        }
        if (socket_ptr->is_close_write())
        {
            // remote shutdown read (write error) before.
            force_close(socket_ptr, sl, result);
        }
        else
        {
            close_read(socket_ptr, result);
        }

        return SOCKET_EVENT_CLOSE;
    }

    if (socket_ptr->is_close_read())
    {
        // discard recv data (rare case: if socket is HALF_CLOSE_READ, reading event is disable.)
        delete[] buf.buf;
        return -1;
    }

    socket_ptr->stat_recv(n, time_);

    result->svc_handle = socket_ptr->svc_handle;
    result->socket_id = socket_ptr->socket_id;
    result->ud = n;
    result->data_ptr = buf.buf;

    return SOCKET_EVENT_DATA;
}

int socket_server::forward_message_udp(socket* socket_ptr, socket_lock& sl, socket_message* result)
{
    socket_addr sa;
    socklen_t sa_sz = sizeof(sa);
    int recv_n = ::recvfrom(socket_ptr->socket_fd, udp_recv_buf_, MAX_UDP_PACKAGE, 0, &sa.addr.s, &sa_sz);
    if (recv_n < 0)
    {
        if (errno == EINTR || errno == AGAIN_WOULDBLOCK)
            return -1;
        
        // close when error
        force_close(socket_ptr, sl, result);
        result->data_ptr = ::strerror(errno);

        return SOCKET_EVENT_ERROR;
    }

    // recv statistics
    socket_ptr->stat_recv(recv_n, time_);

    // 将udp地址信息附加到数据尾部
    uint8_t* data_ptr = nullptr;
    // udp v4
    if (sa_sz == sizeof(sa.addr.v4))
    {
        // socket type must udp v4
        if (socket_ptr->protocol_type != SOCKET_TYPE_UDP)
            return -1;

        data_ptr = new uint8_t[recv_n + 1 + 2 + 4]{0};
        sa.to_udp_address(SOCKET_TYPE_UDP, data_ptr + recv_n);
    }
    // udp v6
    else
    {
        // socket type must udp v6
        if (socket_ptr->protocol_type != SOCKET_TYPE_UDPv6)
            return -1;

        data_ptr = new uint8_t[recv_n + 1 + 2 + 16]{0};
        sa.to_udp_address(SOCKET_TYPE_UDPv6, data_ptr + recv_n);
    }
    ::memcpy(data_ptr, udp_recv_buf_, recv_n);

    result->svc_handle = socket_ptr->svc_handle;
    result->socket_id = socket_ptr->socket_id;
    result->ud = recv_n;
    result->data_ptr = (char*)data_ptr;

    return SOCKET_EVENT_UDP;
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

    if (buf_ptr->type == BUFFER_TYPE_MEMORY)
    {
        send_object_init(so, (void*)buf_ptr->data_ptr, buf_ptr->data_size);
    }
    else if (buf_ptr->type == BUFFER_TYPE_OBJECT)
    {
        send_object_init(so, (void*)buf_ptr->data_ptr, USER_OBJECT);
    }
    else if (buf_ptr->type == BUFFER_TYPE_RAW_POINTER)
    {
        so->buffer = (void*)buf_ptr->data_ptr;
        so->sz = buf_ptr->data_size;
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

}

