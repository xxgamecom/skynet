#pragma once

#include "tcp/tcp_session_i.h"
#include "tcp/tcp_session_handler_i.h"

#include "../base/io_service.h"

#include "base/socket_option_def.h"
#include "tcp_session_write_queue.h"

#include <deque>
#include <mutex>
#include <chrono>
#include <atomic>

namespace skynet::net::impl {

/**
 * tcp session
 *
 * 内部有socket句柄, 用于两端通信
 * 会话内存估算: 只计算收发所需内存(1个读消息缓存, 1个写消息队列), 其他忽略
 * msg_read_buf_size_ + msg_write_buf_size_ * msg_write_queue_size_
 */
class tcp_session_impl : public asio::noncopyable,
                         public tcp_session,
                         public std::enable_shared_from_this<tcp_session_impl>
{
public:
    enum session_state
    {
        SESSION_STATE_OPEN = 1,                                 // session opened
        SESSION_STATE_CLOSING = 2,                              // session closing
        SESSION_STATE_CLOSE = 3,                                // session closed
    };

protected:
    session_id_t session_id_ = INVALID_SESSION_ID;              // session id
    session_state state_ = SESSION_STATE_CLOSE;                 // session state

    std::shared_ptr<asio::ip::tcp::socket> socket_ptr_;         // socket
    std::shared_ptr<tcp_session_handler> event_handler_ptr_;    // event handler (callback)

    std::string remote_addr_ = "";                              // remote address (connect时外部传入的地址, maybe ip or domain)
    uint16_t remote_port_ = 0;                                  // remote port (connect时外部传入的地址)

    // read/write functions
protected:
    size_t msg_read_buf_size_ = 0;                              // read buffer size
    std::shared_ptr<io_buffer> msg_read_buf_ptr_;               // read buffer (读消息只用一个缓存)

    size_t msg_write_buf_size_ = 0;                             // write buffer size
    size_t msg_write_queue_size_ = 0;                           // write buffer queue size
    tcp_session_write_queue msg_write_queue_;                   // write queue (写消息需要用队列, 因为可能写大数据)

    std::chrono::steady_clock::time_point last_read_time_;      // latest read time
    std::chrono::steady_clock::time_point last_write_time_;     // latest write time

    // io statistics
protected:
    std::atomic<int64_t> read_bytes_;                           // total read bytes
    std::atomic<int64_t> write_bytes_;                          // total write bytes
    std::atomic<int64_t> delta_read_bytes_;                     // delta read bytes
    std::atomic<int64_t> delta_write_bytes_;                    // delta write bytes

public:
    tcp_session_impl(int32_t msg_read_buf_size,
                     int32_t msg_write_buf_size,
                     int32_t msg_write_queue_size);
    ~tcp_session_impl() = default;

    // tcp_session impl
public:
    // set event handler (callback)
    void set_event_handler(std::shared_ptr<tcp_session_handler> event_handler_ptr) override;

    // open session
    bool open(std::shared_ptr<io_service> ios_ptr, std::string local_ip = "", uint16_t local_port = 0) override;
    /**
     * close session
     * @param is_force close immediately (不等待剩余数据发送完毕)
     */
    void close(bool is_force = true) override;

    // set/get session id
    void session_id(session_id_t id) override;
    session_id_t session_id() override;

public:
    // post a start read async operation (通常在连接成功后调用, 用于发起异步读操作)
    void start_read() override;

    // write data, 返回写入的字节数(异步写入, 写入成功后通过handle_session_write回调)
    // 这里的内部保护只保证1个线程写的情况是安全的, 具体查看tcp_session_write_queue的说明
    size_t write(const char* data_ptr, size_t data_len) override;

    /**
     * session idle test 用于外部对会话进行闲置测试
     * @param check_type 要检测的闲置类型
     * @param check_seconds 判断闲置的时间(秒)
     */
    void check_idle(idle_type check_type, int32_t check_seconds) override;

public:
    // 会话是否打开
    bool is_open() override;

    // get session socket
    std::shared_ptr<asio::ip::tcp::socket> get_socket() override;

    // read/write statistics
    int64_t read_bytes() override;
    int64_t write_bytes() override;

    // read/write delta statistics (note: will reset count after call)
    int64_t delta_read_bytes() override;
    int64_t delta_write_bytes() override;

    // get remote endpoint info (only valid after connected)
    asio::ip::tcp::endpoint remote_endpoint() override;
    // get local endpoint info (only valid after connected)
    asio::ip::tcp::endpoint local_endpoint() override;

    // socket options
public:
    bool set_sock_option(sock_options opt, int32_t value) override;
    bool get_sock_option(sock_options opt, int32_t& value) override;

    // async post
protected:
    // post an async read operation
    void async_read_once();
    // post an async write operation
    void async_write_once();

    // handle functions
protected:
    // handle a complete read operation
    void handle_async_read(const asio::error_code& ec, size_t bytes_transferred);
    // handle a complete write operation
    void handle_async_write(const asio::error_code& ec, size_t bytes_transferred);
};

}

#include "tcp_session.inl"

