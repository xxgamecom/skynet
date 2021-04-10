#pragma once

#include <string>
#include <memory>
#include <cstdint>

namespace skynet::net {

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
    virtual bool connect(std::shared_ptr<tcp_session> session_ptr, std::string remote_addr, uint16_t remote_port,
                         int32_t timeout_seconds = 0, std::string local_ip = "", uint16_t local_port = 0) = 0;
};

}
