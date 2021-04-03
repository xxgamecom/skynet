#pragma once

#include <cstdint>
#include <memory>
#include <list>

#include "socket.h"
#include "socket_lock.h"
#include "socket_addr.h"
#include "socket_info.h"
#include "socket_pool.h"
#include "buffer.h"
#include "pipe.h"
#include "poller.h"
#include "socket_server_ctrl_cmd.h"

namespace skynet {

struct socket_udp_address;

//
class socket_server
{
private:
    // constants
    enum
    {
        ADDR_TMP_BUFFER_SIZE            = 128,                                          //
        MAX_UDP_PACKAGE                 = 64 * 1024,                                    // udp最大数据包
    };

private:
    volatile uint64_t                   time_ = 0;                                      // used to statistics

    pipe                                pipe_;                                          //
    bool                                need_check_ctrl_cmd_ = true;                    // 是否需要检查控制命令

    //
    poller                              event_poller_;                                  // poller (epoll或kevent的句柄)
    poller::event                       events_[poller::MAX_WAIT_EVENT];                // poller 事件列表 (epoll_wait 返回的事件集合)
    int                                 event_wait_n_ = 0;                              // poller 需要处理的事件数目
    int                                 event_next_index_ = 0;                          // poller 的下一个未处理的事件索引

    //
    socket_object_interface             soi_;                                           //
    socket_pool                         socket_pool_;                                   // socket pool
    uint8_t                             udp_recv_buf_[MAX_UDP_PACKAGE] = { 0 };         //
    char                                addr_tmp_buf_[ADDR_TMP_BUFFER_SIZE] = { 0 };    // 地址信息临时数据

public:
    socket_server() = default;
    ~socket_server();

public:
    // 初始化
    bool init(uint64_t time = 0);
    // 清理
    void fini();

public:
    // bind & listen, return socket id
    int listen(uint64_t svc_handle, const char* addr, int port, int backlog);
    // connect remote server (noblocked), return socket id
    int connect(uint64_t svc_handle, const char* addr, int port);

    // start socket server (must listen before)
    void start(uint64_t svc_handle, int socket_id);
    //
    void pause(uint64_t svc_handle, int socket_id);
    // 退出socket服务
    void exit();

    // 关闭socket服务
    void close(uint64_t svc_handle, int socket_id);
    // 停止socket服务
    void shutdown(uint64_t svc_handle, int socket_id);
    // for tcp
    void nodelay(int socket_id);

    // 将 stdin, stdout 这些IO加入到poller中 (not sockeet bind)
    int bind(uint64_t svc_handle, int fd);

    // 刷新时间
    void update_time(uint64_t time);

    /**
     * 获取网络事件 (底层使用epoll或kqueue)
     * 内部使用循环, 持续监听网络事件
     * 
     * @param result 结果数据存放的地址指针
     * @param is_more 是否有更多事件需要处理
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
     * 创建一个udp socket句柄，并绑定skynet服务的handle，udp不需要像tcp那样要调用start后才能接收消息
     * 如果 port != 0, 绑定socket端口;
     * 如果 addr == NULL, 绑定 ipv4 0.0.0.0;
     * 如果想要使用ipv6，地址使用“::”，端口中port设为0
     */
    int udp(uint64_t svc_handle, const char* addr, int port);

    /**
     * 设置默认的端口地址
     * 
     * @return 0 成功
     */
    int udp_connect(int socket_id, const char* addr, int port);

    /*
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
     * 把要发送的数据写入发送管道，交给socket线程去发送
     * 
     * @param cmd 控制命令数据包
     */
    void _send_ctrl_cmd(ctrl_cmd_package* cmd);
    // 当工作线程执行socket.listen后，socket线程从接收管道读取数据，执行ctrl_cmd
    int _recv_ctrl_cmd(socket_message* result);
    
    // return -1 when connecting
    int handle_ctrl_cmd_open_socket(request_open* cmd, socket_message* result);
    int handle_ctrl_cmd_close_socket(request_close* cmd, socket_message* result);
    int handle_ctrl_cmd_bind_socket(request_bind* cmd, socket_message* result);
    int handle_ctrl_cmd_resume_socket(request_resume_pause* cmd, socket_message* result);
    int handle_ctrl_cmd_pause_socket(request_resume_pause* cmd, socket_message* result);
    int handle_ctrl_cmd_setopt_socket(request_set_opt* cmd);
    int handle_ctrl_cmd_exit_socket(socket_message* result);
    int handle_ctrl_cmd_send_socket(request_send* cmd, socket_message* result, int priority, const uint8_t* udp_address);
    int handle_ctrl_cmd_trigger_write(request_send* cmd, socket_message* result);
    int handle_ctrl_cmd_listen_socket(request_listen* cmd, socket_message* result);
    int handle_ctrl_cmd_add_udp_socket(request_udp* cmd);
    int handle_ctrl_cmd_set_udp_address(request_set_udp* cmd, socket_message* result);

    // send
private:
    //
    int send_write_buffer(socket* socket_ptr, socket_lock& sl, socket_message* result);

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
    int do_send_write_buffer(socket* socket_ptr, socket_lock& sl, socket_message* result);

    // 写缓存列表内的数据是否完整 (write_buffer_list head不完整, 之前发送了部分数据)
    int list_uncomplete(write_buffer_list* wb_list);
    // 将 ‘低优先级’ 写缓存列表的head移到 '高优先级' 写缓存列表内
    void raise_uncomplete(socket* socket_ptr);
    
    //
    int send_write_buffer_list(socket* socket_ptr, write_buffer_list* wb_list, socket_lock& sl, socket_message* result);
    int send_write_buffer_list_tcp(socket* socket_ptr, write_buffer_list* wb_list, socket_lock& sl, socket_message* result);
    int send_write_buffer_list_udp(socket* socket_ptr, write_buffer_list* wb_list, socket_message* result);    

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
    void append_sendbuffer(socket* socket_ptr, request_send* cmd, bool is_high = true, const uint8_t* udp_address = nullptr);

    // 准备发送缓存
    write_buffer* prepare_write_buffer(write_buffer_list* wb_list, request_send* cmd, int size);
    // 清理发送缓存
    void free_write_buffer(write_buffer* wb);
    void free_write_buffer_list(write_buffer_list* wb_list);    

private:
    void close_read(socket* socket_ptr, socket_message* result);
    int close_write(socket* socket_ptr, socket_lock& sl, socket_message* result);

    int enable_write(socket* socket_ptr, bool enable);
    int enable_read(socket* socket_ptr, bool enable);

    //
    void force_close(socket* socket_ptr, socket_lock& sl, socket_message* result);

    // 
    // @param socket_id
    // @param socket_fd socket句柄
    // @param protocol_type
    // @param svc_handle skynet服务句柄
    // @param add 是否加入到event_poller中, 默认true
    socket* new_socket(int socket_id, int socket_fd, int protocol_type, uint64_t svc_handle, bool add = true);

    //
    void drop_udp(socket* socket_ptr, write_buffer_list* wb_list, write_buffer* wb);

    // return 0 when failed, or -1 when file limit
    int handle_accept(socket* socket_ptr, socket_message* result);
    //
    int handle_connect(socket* socket_ptr, socket_lock& sl, socket_message* result);

    // 单个socket每次从内核尝试读取的数据字节数为sz
    // 比如，客户端发了一个1kb的数据，socket线程会从内核里依次读取64b，128b，256b，512b，64b数据，总共需读取5次，即会向gateserver服务发5条消息，一个TCP包被切割成5个数据块。
    // 第5次尝试读取1024b数据，所以可能会读到其他TCP包的数据(只要客户端有发送其他数据)。接下来，客户端再发一个1kb的数据，socket线程只需从内核读取一次即可。
    // return -1 (ignore) when error
    int forward_message_tcp(socket* socket_ptr, socket_lock& sl, socket_message* result);
    //
    int forward_message_udp(socket* socket_ptr, socket_lock& sl, socket_message* result);

    // 初始化send_object
    bool send_object_init(send_object* so, const void* object, size_t sz);
    void send_object_init(send_object* so, send_buffer* buf);

    void _clear_closed_event(int socket_id);    
};

}

#include "socket_server.inl"

