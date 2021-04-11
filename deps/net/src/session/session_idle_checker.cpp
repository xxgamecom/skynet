#include "session_idle_checker.h"
#include "net_manager.h"

namespace skynet::net::impl {

// default check interval, seconds
constexpr uint32_t DEFAULT_CHECK_INTERVAL = 1;

session_idle_checker::session_idle_checker(std::shared_ptr<net_manager> net_manager_ptr,
                                           std::shared_ptr<io_service> ios_ptr)
:
net_manager_ptr_(net_manager_ptr),
ios_ptr_(ios_ptr),
idle_check_timer_(ios_ptr_->get_raw_ios())
{
    assert(net_manager_ptr_ != nullptr);
}

bool session_idle_checker::start(session_idle_type check_type, int32_t idle_interval_seconds)
{
    idle_check_type_ = check_type;
    idle_check_seconds_ = idle_interval_seconds;

    if (idle_check_seconds_ > 0)
    {
        asio::error_code ec;
        idle_check_timer_.expires_from_now(std::chrono::seconds(DEFAULT_CHECK_INTERVAL), ec);
        if (ec)
            return false;

        auto self(shared_from_this());
        idle_check_timer_.async_wait([this, self](const asio::error_code& ec) {
            handle_timeout(ec);
        });
    }

    return true;
}

void session_idle_checker::stop()
{
    if (idle_check_seconds_ > 0)
    {
        idle_check_timer_.cancel();
        idle_check_type_ = IDLE_TYPE_BOTH;
        idle_check_seconds_ = 0;
    }
}

void session_idle_checker::handle_timeout(const asio::error_code& ec)
{
    if (!ec)
    {
        // 检查超时
        std::vector<std::weak_ptr<basic_session>> sessions;
        if (net_manager_ptr_->get_sessions(sessions) > 0)
        {
            for (auto& itr : sessions)
            {
                std::shared_ptr<basic_session> ptr(itr.lock());
                if (ptr != nullptr)
                {
                    ptr->check_idle(idle_check_type_, idle_check_seconds_);
                }
            }
        }

        // next check
        if (idle_check_seconds_ > 0)
        {
            asio::error_code ec;
            idle_check_timer_.expires_from_now(std::chrono::seconds(DEFAULT_CHECK_INTERVAL), ec);
            if (ec)
                return;

            auto self(shared_from_this());
            idle_check_timer_.async_wait([this, self](const asio::error_code& ec) {
                handle_timeout(ec);
            });
        }
    }
}

}

