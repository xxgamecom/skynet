#pragma once

#include <string>
#include <cstdint>
#include <memory>

namespace skynet::net {

/**
 * socket server interface
 */
class socket_server
{
public:
    virtual ~socket_server() = default;

public:
    // init/fini
    virtual bool init() = 0;
    virtual void fini() = 0;

    // create tcp server
    virtual int listen(std::string local_uri, uint32_t svc_handle) = 0;
    virtual int listen(std::string local_ip, uint16_t local_port, uint32_t svc_handle) = 0;
};

}
