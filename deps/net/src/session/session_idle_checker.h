#pragma once

#include "../base/io_service.h"

#include "base/session_def.h"

// forward declare
namespace skynet::net {
class net_manager;
}

namespace skynet::net::impl {

class session_idle_checker : public std::enable_shared_from_this<session_idle_checker>,
                             public asio::noncopyable
{
private:
    std::shared_ptr<net_manager> net_manager_ptr_;              // network manager ptr
    std::shared_ptr<io_service> ios_ptr_;                       // ios和acceptor的公用
    asio::steady_timer idle_check_timer_;                       // idle check timer
    session_idle_type idle_check_type_ = IDLE_TYPE_BOTH;        // idle check type
    int32_t idle_check_seconds_ = 0;                            // idle check interval, =0: means don't check (unit: seconds)

public:
    session_idle_checker(std::shared_ptr<net_manager> net_manager_ptr,
                         std::shared_ptr<io_service> ios_ptr);
    ~session_idle_checker() = default;

public:
    bool start(session_idle_type check_type, int32_t idle_interval_seconds);
    void stop();

private:
    void handle_timeout(const asio::error_code& ec);
};

}

