#pragma once

#include <string>
#include <memory>
#include <cstdint>

namespace skynet { namespace net {

// forward declare
class tcp_session;
class tcp_connector_handler;

/**
 * connector interface
 */
class tcp_connector
{
public:
    virtual ~tcp_connector() = default;

public:
    // set connector event handler
    virtual void set_event_handler(std::shared_ptr<tcp_connector_handler> event_handler_ptr) = 0;

    // connect (timeout include host resolve and connect time)
    virtual bool connect(std::shared_ptr<tcp_session> session_ptr,
                         const std::string remote_addr,
                         const uint16_t remote_port,
                         int32_t timeout_seconds = 0,
                         const std::string local_ip = "",
                         const uint16_t local_port = 0) = 0;
};

} }
