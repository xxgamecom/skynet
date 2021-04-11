#pragma once

#include <cstdint>
#include <memory>
#include <list>

#include "socket_object.h"
#include "socket_lock.h"
#include "socket_addr.h"
#include "socket_info.h"
#include "socket_object_pool.h"
#include "buffer.h"
#include "socket_server_ctrl_cmd.h"
#include "pipe/pipe.h"
#include "poller/poller.h"

namespace skynet {

struct socket_udp_address;

//
class socket_server final
{
private:
    // constants
    enum
    {
        ADDR_TMP_BUFFER_SIZE = 128,                     //
        MAX_UDP_PACKAGE = 64 * 1024,                    // udp最大数据包
    };

private:
    volatile uint64_t time_ticks_ = 0;                  // used to statistics

    pipe pipe_;                                         //
    bool need_check_ctrl_cmd_ = true;                   // 是否需要检查控制命令

    //
    poller event_poller_;                               // poller (epoll或kevent的句柄)
    poller::event events_[poller::MAX_WAIT_EVENT];      // poller 事件列表 (epoll_wait 返回的事件集合)
    int event_wait_n_ = 0;                              // poller 需要处理的事件数目
    int event_next_index_ = 0;                          // poller 的下一个未处理的事件索引

    //
    socket_object_interface soi_;                       //
    socket_object_pool socket_pool_;                    // socket object pool
    uint8_t udp_recv_buf_[MAX_UDP_PACKAGE] = { 0 };     //
    char addr_tmp_buf_[ADDR_TMP_BUFFER_SIZE] = { 0 };   // 地址信息临时数据

public:
    socket_server() = default;
    ~socket_server();

public:
    bool init(uint64_t ticks = 0);
    void fini();

    /**
     * exit socket server
     */
    void exit();

public:
    /**
     * create tcp server, bind & listen
     *
     * @param svc_handle skynet service handle
     * @param local_ip local ip or domain name
     * @param local_port local port
     * @param backlog
     * @return socket id
     */
    int listen(uint32_t svc_handle, std::string local_ip, uint16_t local_port, int32_t backlog);

    /**
     * create tcp client, connect remote server (async)
     *
     * @param svc_handle skynet service handle
     * @param remote_ip remote ip or domain name
     * @param remote_port remote port
     * @return socket id
     */
    int connect(uint32_t svc_handle, std::string remote_ip, uint16_t remote_port);

    /**
     * start tcp server (must listen before)
     *
     * @param svc_handle skynet service handle
     * @param socket_id listen socket id
     */
    void start(uint32_t svc_handle, int socket_id);
    /**
     * pause tcp server
     *
     * @param svc_handle skynet service handle
     * @param socket_id listen socket id
     */
    void pause(uint32_t svc_handle, int socket_id);
    // close socket
    void close(uint32_t svc_handle, int socket_id);
    // shutdown socket
    void shutdown(uint32_t svc_handle, int socket_id);

    // socket options - only for tcp
    void nodelay(int socket_id);

    /**
     * bind os fd (stdin, stdout. not socket bind)
     *
     * @param svc_handle skynet service handle
     * @param os_fd os fd
     * @return socket id
     */
    int bind_os_fd(uint32_t svc_handle, int os_fd);

    /**
     * refresh time (call by time thread)
     *
     * @param time_ticks now ticks
     */
    void update_time(uint64_t time_ticks);

    /**
     * get network event
     * 
     * @param result socket message, 结果数据存放的地址指针
     * @param is_more has more event wait to process
     * @return 返回消息类型，对应于宏定义中的SOCKET_DATA的类型
     */
    int poll_socket_event(socket_message* result, bool& is_more);

    // 查询所有socket信息 (socket_info是一个链表)
    void get_socket_info(std::list<socket_info>& si_list);

    /**
     * 发送数据 (低优先级)
     * 
     * @param buf_ptr 要发送的数据
     * @return -1 error, 0 success
     */
    int send(send_buffer* buf_ptr);
    int send_low_priority(send_buffer* buf_ptr);    

    // udp
public:
    /**
     * create an udp socket
     * - create socket fd, bind skynet service handle, bind local address if local_ip or local_port are provided.
     * - no need to call start() after call this.
     * - for udp client, udp_connect after call this.
     *
     * @param svc_handle
     * @param local_ip bind local ip; if local_ip is empty, bind ipv4 "0.0.0.0"; if "::", bind ipv6.
     * @param local_port bind local port
     * @return socket id
     */
    int udp_socket(uint32_t svc_handle, std::string local_ip, uint16_t local_port);

    /**
     * connect remote udp server
     *
     * @param socket_id udp socket id, @see udp_socket()
     * @param remote_ip remote server ip
     * @param remote_port remote server port
     * @return 0: success, -1: failed
     */
    int udp_connect(int socket_id, const char* remote_ip, int remote_port);

    /**
     * 如果 socket_udp_address 为 NULL, 则使用最后调用 socket_server::udp_connect 时传入的address代替。
     * 也可以使用 send 来发送udp数据
     */
    int udp_send(const socket_udp_address*, send_buffer* buf);

    /**
     * 获取消息内的IP地址 (UDP)
     * 
     * @param msg 消息, 传入的消息必须为 SOCKET_EVENT_UDP 类型
     * @param addr_sz 地址所占字节数 
     */
    const socket_udp_address* udp_address(socket_message* msg, int* addr_sz);

    // ctrl cmd
private:
    /**
     * send ctrl command to pipe (ctrl cmd will process by socket thread)
     * 
     * @param cmd ctrl command package
     */
    void _send_ctrl_cmd(ctrl_cmd_package* cmd);

    // 当工作线程执行socket.listen后，socket线程从接收管道读取数据，执行ctrl_cmd
    int handle_ctrl_cmd(socket_message* result);
    // return -1 when connecting
    int handle_ctrl_cmd_listen_socket(cmd_request_listen* cmd, socket_message* result);
    int handle_ctrl_cmd_connect_socket(cmd_request_connect* cmd, socket_message* result);
    int handle_ctrl_cmd_resume_socket(cmd_request_resume_pause* cmd, socket_message* result);
    int handle_ctrl_cmd_pause_socket(cmd_request_resume_pause* cmd, socket_message* result);
    int handle_ctrl_cmd_close_socket(cmd_request_close* cmd, socket_message* result);
    int handle_ctrl_cmd_bind_socket(cmd_request_bind* cmd, socket_message* result);
    int handle_ctrl_cmd_setopt_socket(cmd_request_set_opt* cmd);
    int handle_ctrl_cmd_exit_socket(socket_message* result);
    int handle_ctrl_cmd_send_socket(cmd_request_send* cmd, socket_message* result, int priority, const uint8_t* udp_address);
    int handle_ctrl_cmd_trigger_write(cmd_request_send* cmd, socket_message* result);
    int handle_ctrl_cmd_udp_socket(cmd_request_udp_socket* cmd);
    int handle_ctrl_cmd_set_udp_address(cmd_request_set_udp* cmd, socket_message* result);

    // send
private:
    //
    int send_write_buffer(socket_object* socket_ptr, socket_lock& sl, socket_message* result);

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
    int do_send_write_buffer(socket_object* socket_ptr, socket_lock& sl, socket_message* result);

    // 写缓存列表内的数据是否完整 (write_buffer_list head不完整, 之前发送了部分数据)
    int list_uncomplete(write_buffer_list* wb_list);
    // 将 ‘低优先级’ 写缓存列表的head移到 '高优先级' 写缓存列表内
    void raise_uncomplete(socket_object* socket_ptr);
    
    //
    int send_write_buffer_list(socket_object* socket_ptr, write_buffer_list* wb_list, socket_lock& sl, socket_message* result);
    int send_write_buffer_list_tcp(socket_object* socket_ptr, write_buffer_list* wb_list, socket_lock& sl, socket_message* result);
    int send_write_buffer_list_udp(socket_object* socket_ptr, write_buffer_list* wb_list, socket_message* result);

    // 是否发送缓存
    void free_send_buffer(send_buffer* buf_ptr);
    // 复制发送缓存
    const void* clone_send_buffer(send_buffer* buf_ptr, size_t* sz);

    /**
     * 添加发送缓存
     * 
     * @param is_high 加入到 '高优先级'发送缓存
     * @param udp_address 发送udp数据时, 附加udp地址到数据尾部
     */
    void append_sendbuffer(socket_object* socket_ptr, cmd_request_send* cmd, bool is_high = true, const uint8_t* udp_address = nullptr);

    // 准备发送缓存
    write_buffer* prepare_write_buffer(write_buffer_list* wb_list, cmd_request_send* cmd, int size);
    // 清理发送缓存
    void free_write_buffer(write_buffer* wb);
    void free_write_buffer_list(write_buffer_list* wb_list);    

private:
    void close_read(socket_object* socket_ptr, socket_message* result);
    int close_write(socket_object* socket_ptr, socket_lock& sl, socket_message* result);

    int enable_write(socket_object* socket_ptr, bool enable);
    int enable_read(socket_object* socket_ptr, bool enable);

    //
    void force_close(socket_object* socket_ptr, socket_lock& sl, socket_message* result);

    // 
    // @param socket_id
    // @param socket_fd socket句柄
    // @param protocol_type
    // @param svc_handle skynet服务句柄
    // @param add 是否加入到event_poller中, 默认true
    socket_object* new_socket(int socket_id, int socket_fd, int protocol_type, uint32_t svc_handle, bool add = true);

    //
    void drop_udp(socket_object* socket_ptr, write_buffer_list* wb_list, write_buffer* wb);

    // return 0 when failed, or -1 when file limit
    int handle_accept(socket_object* socket_ptr, socket_message* result);
    //
    int handle_connect(socket_object* socket_ptr, socket_lock& sl, socket_message* result);

    // 单个socket每次从内核尝试读取的数据字节数为sz
    // 比如，客户端发了一个1kb的数据，socket线程会从内核里依次读取64b，128b，256b，512b，64b数据，总共需读取5次，即会向gateserver服务发5条消息，一个TCP包被切割成5个数据块。
    // 第5次尝试读取1024b数据，所以可能会读到其他TCP包的数据(只要客户端有发送其他数据)。接下来，客户端再发一个1kb的数据，socket线程只需从内核读取一次即可。
    // return -1 (ignore) when error
    int forward_message_tcp(socket_object* socket_ptr, socket_lock& sl, socket_message* result);
    //
    int forward_message_udp(socket_object* socket_ptr, socket_lock& sl, socket_message* result);

    // 初始化send_object
    bool send_object_init(send_object* so, const void* object, size_t sz);
    void send_object_init(send_object* so, send_buffer* buf);

    void _clear_closed_event(int socket_id);    
};

}


