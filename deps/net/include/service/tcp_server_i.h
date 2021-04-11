#pragma once

#include <string>
#include <cstdint>
#include <memory>

namespace skynet::net {

class io_service;
class tcp_server_handler;
class tcp_server_acceptor_config;
class tcp_server_session_config;
class io_statistics;
class net_manager;

// tcp服务端
class tcp_server
{
public:
    virtual ~tcp_server() = default;

public:
    virtual bool init() = 0;
    virtual void fini() = 0;

    // set event handler
    virtual void set_event_handler(std::shared_ptr<tcp_server_handler> event_handler_ptr) = 0;

public:
    // start/close
    virtual bool open(std::string local_uri) = 0;
    virtual bool open(std::string local_ip, uint16_t local_port) = 0;
    virtual bool open(std::initializer_list<std::pair<std::string, uint16_t>> local_endpoints) = 0;
    virtual void close() = 0;
};

}
