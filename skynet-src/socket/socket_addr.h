#pragma once

#include <string>
#include <arpa/inet.h>

namespace skynet {

/**
 * socket endpoint info
 *
 */
class socket_addr
{
public:
    union
    {
        struct sockaddr s;
        struct sockaddr_in v4;
        struct sockaddr_in6 v6;
    } addr {{ 0 }};

public:
    // ip:port string
    std::string to_string() const;
    // ip:string string
    bool to_string(char* buf_ptr, size_t buf_sz) const;

    // udp_address convert to socket_addr
    int from_udp_address(int protocol_type, const uint8_t* udp_address);
    // socket_addr convert to udp_address
    int to_udp_address(int protocol_type, uint8_t* udp_address) const;
};

}

