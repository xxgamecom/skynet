#pragma once

#include "base/session_def.h"

#include <string>
#include <cstdint>
#include <memory>

namespace skynet::net {

class io_buffer;
class tcp_session;

/**
 * socket server event handler (callback)
 */
class socket_server_handler
{
public:
    virtual ~socket_server_handler() = default;

    // tcp event callback
public:
    // accept client callback
    virtual void handle_accept(std::shared_ptr<tcp_session> session_ptr) = 0;

    // tcp session read complete callback
    virtual void handle_session_read(std::shared_ptr<tcp_session> session_ptr, char* data_ptr, size_t data_len) = 0;
    // tcp session write complete callback
    virtual void handle_session_write(std::shared_ptr<tcp_session> session_ptr, char* data_ptr, size_t data_len) = 0;
    // tcp session idle callback
    virtual void handle_session_idle(std::shared_ptr<tcp_session> session_ptr, idle_type type) = 0;
    // tcp session close callback
    virtual void handle_sessoin_close(std::shared_ptr<tcp_session> session_ptr) = 0;
};

}
