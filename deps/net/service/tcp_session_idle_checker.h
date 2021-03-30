#pragma once

#include "../core/io_service.h"
#include "../transport/tcp_session_def.h"

namespace skynet { namespace network {

class tcp_session_manager;

class tcp_session_idle_checker final : public std::enable_shared_from_this<tcp_session_idle_checker>
{
private:
    enum { CHECK_INTERVAL = 1 };

private:
    std::shared_ptr<tcp_session_manager> session_manager_ptr_;      // 会话管理引用
    std::shared_ptr<io_service> ios_ptr_;                           // ios和acceptor的公用
    asio::steady_timer idle_check_timer_;                           // idle check timer
    idle_type idle_check_type_ = IDLE_TYPE_BOTH;                    // idle check type
    int32_t idle_check_seconds_ = 0;                                // 用于判定为闲置的时间, 为0时不检测(单位: 秒)

public:
    tcp_session_idle_checker(std::shared_ptr<tcp_session_manager> session_manager_ptr,
                             std::shared_ptr<io_service> ios_ptr);
    ~tcp_session_idle_checker() = default;

public:
    bool start(idle_type check_type, int32_t idle_interval_seconds);
    void stop();

private:
    void handle_timeout(const asio::error_code& ec);

    // noncopyable
private:
    tcp_session_idle_checker(const tcp_session_idle_checker&) = delete;
    tcp_session_idle_checker& operator=(const tcp_session_idle_checker&) = delete;
};

} }

