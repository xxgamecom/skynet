#pragma once

#include "../base/noncopyable.h"

#include "../core/io_service.h"
#include "../core/socket_option_def.h"

#include "tcp_session_def.h"
#include "tcp_session_handler.h"
#include "tcp_session_write_queue.h"

#include <deque>
#include <mutex>
#include <chrono>
#include <atomic>

namespace skynet { namespace net {

// tcp会话, 内部有socket句柄, 用于两端通信
// 会话内存估算: 只计算收发所需内存(1个读消息缓存, 1个写消息队列), 其他忽略
// msg_read_buf_size_ + msg_write_buf_size_ * msg_write_queue_size_
class tcp_session : private noncopyable,
                    public std::enable_shared_from_this<tcp_session>
{
public:
    // 会话状态
    enum session_state
    {
        SESSION_STATE_OPEN = 1,                                 // 会话打开
        SESSION_STATE_CLOSING = 2,                              // 会话正在关闭
        SESSION_STATE_CLOSE = 3,                                // 会话关闭
    };

protected:
    session_id_t session_id_ = INVALID_SESSION_ID;              // 会话id
    session_state state_ = SESSION_STATE_CLOSE;                 // session状态

    std::shared_ptr<asio::ip::tcp::socket> socket_ptr_;         // socket
    std::shared_ptr<tcp_session_handler> event_handler_ptr_;    // 外部tcp会话处理器

    std::string remote_addr_ = "";                              // 远程地址(connect时外部传入的地址, 可能是ip或域名等)
    uint16_t remote_port_ = 0;                                  // 远程端口(connect时外部传入的地址, 可能是ip或域名等)

    // 读写操作相关
protected:
    size_t msg_read_buf_size_ = 0;                              // 读消息缓存大小
    std::shared_ptr<io_buffer> msg_read_buf_ptr_;               // 读消息缓存(读消息只用一个缓存)

    size_t msg_write_buf_size_ = 0;                             // 写消息缓存大小
    size_t msg_write_queue_size_ = 0;                           // 写消息队列池大小
    tcp_session_write_queue msg_write_queue_;                   // 写消息队列(写消息需要用队列, 因为可能写大数据)

    std::chrono::steady_clock::time_point last_read_time_;      // 最后读时间
    std::chrono::steady_clock::time_point last_write_time_;     // 最后写时间

    // IO统计量
protected:
    std::atomic<int64_t> read_bytes_;                           // 读字节计数
    std::atomic<int64_t> write_bytes_;                          // 写字节计数
    std::atomic<int64_t> delta_read_bytes_;                     // 增量读字节数
    std::atomic<int64_t> delta_write_bytes_;                    // 增量写字节数

public:
    tcp_session(int32_t msg_read_buf_size,
                int32_t msg_write_buf_size,
                int32_t msg_write_queue_size);
    ~tcp_session() = default;

public:
    // 设置会话事件处理器
    void set_event_handler(std::shared_ptr<tcp_session_handler> event_handler_ptr);

public:
    // 打开会话
    bool open(std::shared_ptr<io_service> ios_ptr, std::string local_ip = "", uint16_t local_port = 0);
    // 关闭会话(is_force: 是否立即关闭, 如果立即关闭不等待剩余数据发送完毕)
    void close(bool is_force = true);

    // 会话ID
    void session_id(session_id_t id);
    session_id_t session_id();

    // 开始读取数据(通常在连接成功后调用, 用于发起异步读操作)
    void start_read();

    // 写入数据, 返回写入的字节数(异步写入, 写入成功后通过handle_session_write回调)
    // 这里的内部保护只保证1个线程写的情况是安全的, 具体查看tcp_session_write_queue的说明
    size_t write(const char* data_ptr, size_t data_len);

    // 用于外部对会话进行闲置测试(check_type为要检测的闲置类型, check_seconds为判断闲置的时间(秒))
    void check_idle(idle_type check_type, int32_t check_seconds);

public:
    // 会话是否打开
    bool is_open();

    // 获取会话socket
    std::shared_ptr<asio::ip::tcp::socket> get_socket();

    // 读写字节计数
    int64_t read_bytes();
    int64_t write_bytes();

    // 增量读写字节数(注意: 调用后增量重新计数)
    int64_t delta_read_bytes();
    int64_t delta_write_bytes();

    // 获取远端端点信息(连接后才有效)
    asio::ip::tcp::endpoint remote_endpoint();
    // 获取本地端点信息(连接后才有效)
    asio::ip::tcp::endpoint local_endpoint();

    // socket选项
public:
    bool set_sock_option(sock_options opt, int32_t value);
    bool get_sock_option(sock_options opt, int32_t& value);

    // 异步投递
protected:
    // 投递一次异步读操作
    void async_read_once();
    // 投递一个异步写操作
    void async_write_once();

    // 处理函数
protected:
    // 处理完成的读操作
    void handle_async_read(const asio::error_code& ec, size_t bytes_transferred);
    // 处理完成的写操作
    void handle_async_write(const asio::error_code& ec, size_t bytes_transferred);
};

} }

#include "tcp_session.inl"

