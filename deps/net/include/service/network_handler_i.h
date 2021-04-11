#pragma once

#include "base/session_def.h"

#include <string>
#include <cstdint>
#include <memory>

namespace skynet::net {

class io_buffer;
class tcp_session;

/**
 * network tcp server event handler (callback)
 */
class network_tcp_server_handler
{
public:
    virtual ~network_tcp_server_handler() = default;

    // tcp server event callback
public:
    // accept client callback
    virtual void handle_accept(std::shared_ptr<tcp_session> session_ptr) = 0;

    // tcp session read complete callback
    virtual void handle_session_read(std::shared_ptr<tcp_session> session_ptr, char* data_ptr, size_t data_len) = 0;
    // tcp session write complete callback
    virtual void handle_session_write(std::shared_ptr<tcp_session> session_ptr, char* data_ptr, size_t data_len) = 0;
    // tcp session idle callback
    virtual void handle_session_idle(std::shared_ptr<tcp_session> session_ptr, session_idle_type type) = 0;
    // tcp session close callback
    virtual void handle_sessoin_close(std::shared_ptr<tcp_session> session_ptr) = 0;
};

/**
 * network tcp client event handler (callback)
 */
class network_tcp_client_handler
{
public:
    virtual ~network_tcp_client_handler() = default;

public:
    // connect remote success callback
    virtual void handle_tcp_client_connect_success(std::shared_ptr<tcp_session> session_ptr) = 0;
    // connect remote failed callback
    virtual void handle_tcp_client_connect_failed(std::shared_ptr<tcp_session> session_ptr, int32_t err_code, std::string err_msg) = 0;
    // connect remote timeout callback
    virtual void handle_tcp_client_connect_timeout(std::shared_ptr<tcp_session> session_ptr) = 0;

    // tcp client session read complete callback
    virtual void handle_tcp_client_read(std::shared_ptr<tcp_session> session_ptr, char* data_ptr, size_t data_len) = 0;
    // tcp client session write complete callback
    virtual void handle_tcp_client_write(std::shared_ptr<tcp_session> session_ptr, char* data_ptr, size_t data_len) = 0;
    // tcp client session close callback
    virtual void handle_tcp_client_close(std::shared_ptr<tcp_session> session_ptr) = 0;
};

/**
 * network udp server event handler (callback)
 */
class network_udp_server_handler
{
public:
    virtual ~network_udp_server_handler() = default;

public:

};

/**
 * network udp client event handler (callback)
 */
class network_udp_client_handler
{
public:
    virtual ~network_udp_client_handler() = default;

public:
};

}
