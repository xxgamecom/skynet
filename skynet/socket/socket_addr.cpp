#include "socket_addr.h"
#include "server_def.h"

#include <iostream>

namespace skynet { namespace socket {

bool to_endpoint(const socket_addr* sa, char* buf_ptr, size_t buf_sz)
{
    // ip & port
    void* sin_addr = (sa->s.sa_family == AF_INET) ? (void*)&sa->v4.sin_addr : (void*)&sa->v6.sin6_addr;
    int sin_port = ntohs((sa->s.sa_family == AF_INET) ? sa->v4.sin_port : sa->v6.sin6_port);

    // 将数值格式ip地址 转 字符串ip地址
    char tmp[INET6_ADDRSTRLEN];
    if (::inet_ntop(sa->s.sa_family, sin_addr, tmp, sizeof(tmp)) == nullptr)
    {
        buf_ptr[0] = '\0';
        return false;
    }

    // endpoint string format: ip:port
    ::snprintf(buf_ptr, buf_sz, "%s:%d", tmp, sin_port);
    return true;
}

int udp_address_to_sockaddr(int protocol, const uint8_t* udp_address, socket_addr& sa)
{
    // protocol: 1 byte
    int type = (uint8_t)udp_address[0];
    if (type != protocol)
        return 0;

    // port: 2 bytes
    uint16_t port = 0;
    ::memcpy(&port, udp_address+1, sizeof(uint16_t));

    // udp v4
    if (protocol == protocol_type::UDP)
    {
        ::memset(&sa.v4, 0, sizeof(sa.v4));
        sa.s.sa_family = AF_INET;
        sa.v4.sin_port = port;
        
        // udp addr v4: 4 bytes
        ::memcpy(&sa.v4.sin_addr, udp_address + 1 + sizeof(uint16_t), sizeof(sa.v4.sin_addr));
        return sizeof(sa.v4);
    }
    // udp v6
    else if (protocol == protocol_type::UDPv6)
    {
        ::memset(&sa.v6, 0, sizeof(sa.v6));
        sa.s.sa_family = AF_INET6;
        sa.v6.sin6_port = port;

        // udp addr v6: 16 bytes
        ::memcpy(&sa.v6.sin6_addr, udp_address + 1 + sizeof(uint16_t), sizeof(sa.v6.sin6_addr));
        return sizeof(sa.v6);
    }

    return 0;
}

/**
 * 生成的addr_udp的内存结构:
 * 
 * for UDPv4: 占用 1 + 2 + 4 字节
 *    1 byte     2 bytes       4 bytes
 * +----------+----------+----------------+
 * | protocol |  port    |  v4.sin_addr   |
 * +----------+----------+----------------+
 * 
 * for UDPv6: 占用 1 + 2 + 16 字节
 *    1 byte     2 bytes      16 bytes
 * +----------+----------+----------------+
 * | protocol |  port    |  v6.sin_addr   |
 * +----------+----------+----------------+
 */
int sockaddr_to_udp_address(int protocol, const socket_addr* sa, uint8_t* udp_address)
{
    // 1 byte
    udp_address[0] = (uint8_t)protocol;

    int addr_sz = 1;

    // udp v4
    if (protocol == protocol_type::UDP)
    {
        // 2 bytes
        ::memcpy(udp_address + addr_sz, &sa->v4.sin_port, sizeof(sa->v4.sin_port));
        addr_sz += sizeof(sa->v4.sin_port);
        // 4 bytes
        ::memcpy(udp_address + addr_sz, &sa->v4.sin_addr, sizeof(sa->v4.sin_addr));
        addr_sz += sizeof(sa->v4.sin_addr);
    }
    // udp v6
    else
    {
        // 2 bytes
        ::memcpy(udp_address + addr_sz, &sa->v6.sin6_port, sizeof(sa->v6.sin6_port));
        addr_sz += sizeof(sa->v6.sin6_port);

        // 16 bytes
        ::memcpy(udp_address + addr_sz, &sa->v6.sin6_addr, sizeof(sa->v6.sin6_addr));
        addr_sz += sizeof(sa->v6.sin6_addr);
    }
    
    return addr_sz;
}


} }
