#include "io_statistics.h"
#include "tcp_session_manager.h"

namespace skynet { namespace network {

io_statistics::io_statistics(std::shared_ptr<tcp_session_manager> session_manager_ptr,
                             std::shared_ptr<io_service> ios_ptr)
    :
    session_manager_ptr_(session_manager_ptr),
    ios_ptr_(ios_ptr),
    calc_timer_(ios_ptr_->get_raw_ios()),
    read_bytes_(0),
    write_bytes_(0),
    last_read_bytes_(0),
    last_write_bytes_(0)
{
    assert(session_manager_ptr_ != nullptr);
}

bool io_statistics::start()
{
    asio::error_code ec;
    calc_timer_.expires_from_now(boost::posix_time::seconds(UPDATE_READ_WRITE_INTERVAL_SECONDS), ec);
    if (ec)
    {
        return false;
    }

    calc_timer_.async_wait(std::bind(&io_statistics::handle_timeout,
                                     shared_from_this(), std::placeholders::_1));

    reset();

    return true;
}

void io_statistics::stop()
{
    calc_timer_.cancel();
}

// 重置
void io_statistics::reset()
{
    read_bytes_.store(0, std::memory_order_relaxed);
    write_bytes_.store(0, std::memory_order_relaxed);

    last_read_bytes_.store(0, std::memory_order_relaxed);
    last_write_bytes_.store(0, std::memory_order_relaxed);

    std::lock_guard<std::mutex> lock(throughput_update_mutex_);

    read_bytes_throughput_ = 0;
    write_bytes_throughput_ = 0;

    largest_read_bytes_throughput_ = 0;
    largest_write_bytes_throughput_ = 0;

    timeout_count_ = 0;
}

void io_statistics::handle_timeout(const asio::error_code& ec)
{
    if (!ec)
    {
        int64_t read_bytes = 0;
        int64_t write_bytes = 0;
        // 更新总读写
        std::vector<std::weak_ptr<tcp_session>> sessions;
        if (session_manager_ptr_->get_sessions(sessions) > 0)
        {
            for (auto& itr : sessions)
            {
                read_bytes = read_bytes_.load(std::memory_order_relaxed);
                write_bytes = write_bytes_.load(std::memory_order_relaxed);

                std::shared_ptr<tcp_session> ptr(itr.lock());
                if (ptr != nullptr)
                {
                    read_bytes_.compare_exchange_weak(read_bytes, read_bytes + ptr->delta_read_bytes(), std::memory_order_relaxed);
                    write_bytes_.compare_exchange_weak(write_bytes, write_bytes + ptr->delta_write_bytes(), std::memory_order_relaxed);
                }
            }
        }

        ++timeout_count_;

        // 更新吞吐量
        update_throughput();

        // 启动下一次计算
        asio::error_code ec;
        calc_timer_.expires_from_now(boost::posix_time::seconds(UPDATE_READ_WRITE_INTERVAL_SECONDS), ec);
        if (ec)
        {
            return;
        }

        calc_timer_.async_wait(std::bind(&io_statistics::handle_timeout,
                                         shared_from_this(), std::placeholders::_1));
    }
}

}}
