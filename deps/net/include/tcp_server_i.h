#pragma once

#include <string>
#include <cstdint>
#include <memory>

namespace skynet { namespace net {

class tcp_server_handler;

// tcp服务端
class tcp_server
{
public:
    virtual ~tcp_server() = default;

public:
    // set event handler
    virtual void set_event_handler(std::shared_ptr<tcp_server_handler> event_handler_ptr) = 0;

public:
    // start/close
    virtual bool open(const std::string local_uri, bool is_reuse_addr = true) = 0;
    virtual bool open(const std::string local_ip, const uint16_t local_port, bool is_reuse_addr = true) = 0;
    virtual bool open(std::initializer_list<std::pair<std::string, uint16_t>> local_endpoints, bool is_reuse_addr = true) = 0;
    virtual void close() = 0;

//    // get config
//    tcp_server_acceptor_config& get_acceptor_config();
//    tcp_server_session_config& get_session_config();
//
//    // get io statistics
//    std::shared_ptr<io_statistics> get_io_statistics();
};

} }
