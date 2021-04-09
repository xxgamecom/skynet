#pragma once

#include "tcp/tcp_acceptor_def.h"
#include "base/socket_option_def.h"

namespace skynet::net {

// forward declare
class tcp_session;
class tcp_acceptor_handler;

/**
 * tcp acceptor (passive connector)
 */
class tcp_acceptor
{
public:
    virtual ~tcp_acceptor() = default;

public:
    // set accept event handler
    virtual void set_event_handler(std::shared_ptr<tcp_acceptor_handler> event_handler_ptr) = 0;

public:
    // open acceptor (bind & listen)
    virtual bool open(const std::string local_ip, uint16_t local_port, bool reuse_addr = true, int32_t backlog = DEFAULT_BACKLOG) = 0;
    // close acceptor
    virtual void close() = 0;

    // get skynet service handle
    virtual uint32_t svc_handle() = 0;

    // post an async accept
    virtual void accept_once(std::shared_ptr<tcp_session> session_ptr) = 0;

    // local endpoint info
    virtual asio::ip::tcp::endpoint local_endpoint() const = 0;

    // socket options
public:
    virtual bool set_sock_option(sock_options opt, int32_t value) = 0;
    virtual bool get_sock_option(sock_options opt, int32_t& value) = 0;
};

}
