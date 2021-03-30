#include "tcp_session_idle_checker.h"
#include "tcp_session_manager.h"

namespace skynet { namespace network {

tcp_session_idle_checker::tcp_session_idle_checker(std::shared_ptr<tcp_session_manager> session_manager_ptr,
                                                   std::shared_ptr<io_service> ios_ptr)
:
session_manager_ptr_(session_manager_ptr),
ios_ptr_(ios_ptr),
idle_check_timer_(ios_ptr_->get_raw_ios())
{
    assert(session_manager_ptr_ != nullptr);
}

bool tcp_session_idle_checker::start(idle_type check_type, int32_t idle_interval_seconds)
{
    idle_check_type_ = check_type;
    idle_check_seconds_ = idle_interval_seconds;

    if (idle_check_seconds_ > 0)
    {
        asio::error_code ec;
        idle_check_timer_.expires_from_now(std::chrono::seconds(CHECK_INTERVAL), ec);
        if (ec) return false;

        idle_check_timer_.async_wait(std::bind(&tcp_session_idle_checker::handle_timeout,
                                               shared_from_this(), std::placeholders::_1));
    }

    return true;
}

void tcp_session_idle_checker::stop()
{
    if (idle_check_seconds_ > 0)
    {
        idle_check_timer_.cancel();
        idle_check_type_ = IDLE_TYPE_BOTH;
        idle_check_seconds_ = 0;
    }
}

void tcp_session_idle_checker::handle_timeout(const asio::error_code& ec)
{
    if (!ec)
    {
        // 检查超时
        std::vector<std::weak_ptr<tcp_session>> sessions;
        if (session_manager_ptr_->get_sessions(sessions) > 0)
        {
            for (auto& itr : sessions)
            {
                std::shared_ptr<tcp_session> ptr(itr.lock());
                if (ptr != nullptr)
                {
                    ptr->check_idle(idle_check_type_, idle_check_seconds_);
                }
            }
        }

        // 启动下一次检测
        if (idle_check_seconds_ > 0)
        {
            asio::error_code ec;
            idle_check_timer_.expires_from_now(std::chrono::seconds(CHECK_INTERVAL), ec);
            if (ec) return;

            idle_check_timer_.async_wait(std::bind(&tcp_session_idle_checker::handle_timeout,
                                                   shared_from_this(), std::placeholders::_1));
        }
    }
}

} }

