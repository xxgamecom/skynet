#pragma once

#include "../core/io_service.h"
#include "../core/time_helper.h"

#include "fmt/format.h"

#include <cstdint>
#include <atomic>
#include <mutex>

namespace skynet { namespace network {

class tcp_session_manager;

// IO常用统计量
class io_statistics : public std::enable_shared_from_this<io_statistics>
{
private:
    enum
    {
        UPDATE_READ_WRITE_INTERVAL_SECONDS = 1,                     // 更新读写计数周期
        THROUGHPUT_INTERVAL_SECONDS = 3,                            // 吞吐计算间隔(默认3秒)
    };

private:
    std::shared_ptr<tcp_session_manager> session_manager_ptr_;      // 会话管理引用
    std::shared_ptr<io_service> ios_ptr_;                           // ios和acceptor的公用
    asio::deadline_timer calc_timer_;                               // 计算吞吐量定时器

    int32_t timeout_count_ = 0;                                     // 超时计数(用于计算控制吞吐时间)

    // 读写
private:
    std::atomic<int64_t> read_bytes_;                               // 读字节数
    std::atomic<int64_t> write_bytes_;                              // 写字节数

    std::atomic<int64_t> last_read_bytes_;                          // 最近读字节数
    std::atomic<int64_t> last_write_bytes_;                         // 最近写字节数

    // 吞吐量
private:
    std::mutex throughput_update_mutex_;                            // 吞吐量更新保护

    double read_bytes_throughput_ = 0;                              // 读字节数吞吐量(每秒读字节数)
    double write_bytes_throughput_ = 0;                             // 写字节数吞吐量(每秒写字节数)

    double largest_read_bytes_throughput_ = 0;                      // 最大读吞吐量
    double largest_write_bytes_throughput_ = 0;                     // 最大写吞吐量

public:
    io_statistics(std::shared_ptr<tcp_session_manager> session_manager_ptr,
                  std::shared_ptr<io_service> ios_ptr);
    ~io_statistics() = default;

public:
    bool start();
    void stop();
    void reset();

    // 吞吐量
public:
    // 更新吞吐量计数器
    void update_throughput();

    // 读写字节数
    int64_t read_bytes();
    int64_t write_bytes();

    // 读写字节数吞吐量(每秒字节数)
    double read_bytes_throughput();
    double write_bytes_throughput();

    // 最大读写字节数吞吐量(每秒读字节数)
    double largest_read_bytes_throughput();
    double largest_write_bytes_throughput();

private:
    void handle_timeout(const asio::error_code& ec);

    // noncopyable
private:
    io_statistics(const io_statistics&) = delete;
    io_statistics& operator=(const io_statistics&) = delete;
};

} }

#include "io_statistics.inl"

