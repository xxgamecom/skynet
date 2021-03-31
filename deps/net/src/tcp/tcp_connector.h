#pragma once

#include "../core/io_service.h"

#include "tcp_connector_i.h"
#include "tcp_connector_handler_i.h"

#include <string>
#include <memory>
#include <cstdint>


namespace skynet { namespace net {

// forward declare
class tcp_session;

namespace impl {

/**
 * tcp connector
 * after successful connection, the external incoming tcp session object will be initialized
 */
class tcp_connector_impl : public asio::noncopyable,
                           public tcp_connector,
                           public std::enable_shared_from_this<tcp_connector_impl>
{
protected:
    std::shared_ptr<io_service> ios_ptr_;                       // ios (from io_service_pool)

    asio::ip::tcp::resolver resolver_;                          // address resolver
    std::shared_ptr<asio::steady_timer> connect_timer_ptr_;     // connect timer
    bool is_connecting_ = false;

    std::shared_ptr<tcp_connector_handler> event_handler_ptr_;

public:
    explicit tcp_connector_impl(std::shared_ptr<io_service> ios_ptr);
    ~tcp_connector_impl() = default;

    // tcp_connector impl
public:
    // 设置连接事件处理器
    void set_event_handler(std::shared_ptr<tcp_connector_handler> event_handler_ptr) override ;

    // 连接(超时包括地址解析和实际连接所需时间)
    bool connect(std::shared_ptr<tcp_session> session_ptr,
                 const std::string remote_addr,
                 const uint16_t remote_port,
                 int32_t timeout_seconds = 0,
                 const std::string local_ip = "",
                 const uint16_t local_port = 0) override;

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

    // handle timeout
    void handle_timeout(std::shared_ptr<tcp_session> session_ptr, const asio::error_code& ec);
};

} } }

#include "tcp_connector.inl"

