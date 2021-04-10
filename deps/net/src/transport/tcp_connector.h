#pragma once

#include "../base/io_service.h"

#include "transport/tcp_connector_i.h"
#include "transport/tcp_connector_handler_i.h"

#include <string>
#include <memory>
#include <cstdint>


// forward declare
namespace skynet::net {
class tcp_session;
}

namespace skynet::net::impl {

/**
 * tcp connector
 * after successful connection, the external incoming tcp session object will be initialized
 */
class tcp_connector_impl : public tcp_connector,
                           public std::enable_shared_from_this<tcp_connector_impl>,
                           public asio::noncopyable
{
protected:
    std::shared_ptr<io_service> ios_ptr_;                       // ios (from io_service_pool), must provider

    asio::ip::tcp::resolver resolver_;                          // host resolver
    std::shared_ptr<asio::steady_timer> connect_timer_ptr_;     // connect timer, for resolve & connect timeout
    bool is_connecting_ = false;                                // connecting flag

    std::shared_ptr<tcp_connector_handler> event_handler_ptr_;  // event handler (callback)

public:
    explicit tcp_connector_impl(std::shared_ptr<io_service> ios_ptr);
    ~tcp_connector_impl() override = default;

    // tcp_connector impl
public:
    // set event handler
    void set_event_handler(std::shared_ptr<tcp_connector_handler> event_handler_ptr) override;

    // connect remote endpoint(timeout include resolve & connect time)
    bool connect(std::shared_ptr<tcp_session> session_ptr,
                 std::string remote_addr,
                 uint16_t remote_port,
                 int32_t timeout_seconds = 0,
                 std::string local_ip = "",
                 uint16_t local_port = 0) override;

    // connect timer
protected:
    // start/stop connect timer
    bool start_connect_timer(std::shared_ptr<tcp_session> session_ptr, int32_t timeout_seconds);
    void stop_connect_timer();

    // handle function
protected:
    // handle async address resolve
    void handle_async_resolve(std::shared_ptr<tcp_session> session_ptr,
                              const asio::error_code& ec,
                              asio::ip::tcp::resolver::iterator endpoint_itr);
    // handle async connect
    void handle_async_connect(std::shared_ptr<tcp_session> session_ptr, const asio::error_code& ec);

    // handle connect timeout
    void handle_connect_timeout(std::shared_ptr<tcp_session> session_ptr, const asio::error_code& ec);
};

}

#include "tcp_connector.inl"

