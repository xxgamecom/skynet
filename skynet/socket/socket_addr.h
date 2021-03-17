#pragma once

#include <arpa/inet.h>

namespace skynet {

// socket address
union socket_addr
{
    struct sockaddr             s;
    struct sockaddr_in          v4;
    struct sockaddr_in6         v6;
};

// socket_addr to endpoint info(ip:port)
bool to_endpoint(const socket_addr* sa, char* buf_ptr, size_t buf_sz);

// udp address to socket_addr, return socket_addr length
int udp_address_to_socket_addr(int protocol, const uint8_t* udp_address, socket_addr& sa);

// socket_addr to udp address, return udp address length
int socket_addr_to_udp_address(int protocol, const socket_addr* sa, uint8_t* udp_address);

}

