#pragma once

#include <arpa/inet.h>

namespace skynet { namespace socket {

// sockaddr
union socket_addr
{
    struct sockaddr             s;
    struct sockaddr_in          v4;
    struct sockaddr_in6         v6;
};

/**
 * sockaddr 转 端点信息(ip:port)
 */
bool to_endpoint(const socket_addr* sa, char* buf_ptr, size_t buf_sz);

/**
 * udp address 转 sockaddr
 * 
 * @param protocol
 * @param udp_address
 * @param sa 输出的 sockaddr
 */
int udp_address_to_sockaddr(int protocol, const uint8_t* udp_address, socket_addr& sa);

/**
 * sockaddr 转 udp address
 * 
 * @param protocol
 * @param sa
 * @param udp_address 输出的udp address
 * 
 * @return udp地址长度
 */
int sockaddr_to_udp_address(int protocol, const socket_addr* sa, uint8_t* udp_address);

} }


