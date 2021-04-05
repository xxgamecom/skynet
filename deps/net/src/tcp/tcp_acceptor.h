#pragma once

#include "base/socket_option_def.h"

#include "../base/io_service.h"

#include "tcp/tcp_acceptor_def.h"
#include "tcp/tcp_acceptor_i.h"
#include "tcp/tcp_acceptor_handler_i.h"


// forward declare
namespace skynet::net {
class tcp_session;
}

namespace skynet::net::impl {

/**
 * tcp passitive connector (used to accept the connection from remote)
 */
class tcp_acceptor_impl : public tcp_acceptor,
                          public std::enable_shared_from_this<tcp_acceptor_impl>,
                          public asio::noncopyable
{
protected:
    std::shared_ptr<io_service> ios_ptr_;                       // io service, only used for acceptor
    std::shared_ptr<asio::ip::tcp::acceptor> acceptor_ptr_;     // tcp acceptor
    std::shared_ptr<tcp_acceptor_handler> event_handler_ptr_;   // event handler

public:
    explicit tcp_acceptor_impl(std::shared_ptr<io_service> ios_ptr,
                               std::shared_ptr<tcp_acceptor_handler> event_handler_ptr = nullptr);
    ~tcp_acceptor_impl() override = default;

    // tcp_acceptor impl
public:
    // set event handler (event callback)
    void set_event_handler(std::shared_ptr<tcp_acceptor_handler> event_handler_ptr) override;

    bool open(const std::string local_ip, uint16_t local_port, bool is_reuse_addr = true, int32_t backlog = DEFAULT_BACKLOG) override;
    void close() override;

public:
    // post an async accept operation
    void accept_once(std::shared_ptr<tcp_session> session_ptr) override;

    // get local endpoint info
    asio::ip::tcp::endpoint local_endpoint() const override;

    // socket options
public:
    bool set_sock_option(sock_options opt, int32_t value) override;
    bool get_sock_option(sock_options opt, int32_t& value) override;

protected:
    // handle accept
    void handle_async_accept(std::shared_ptr<tcp_session> session_ptr, const asio::error_code& ec);
};

}


