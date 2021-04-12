#include "socket_endpoint.h"
#include "socket_object.h"

#include "fmt/format.h"

#include <cstring>

namespace skynet {

std::string socket_endpoint::to_string() const
{
    // ip & port
    void* sin_addr = (addr.s.sa_family == AF_INET) ? (void*)&addr.v4.sin_addr : (void*)&addr.v6.sin6_addr;
    int sin_port = ntohs((addr.s.sa_family == AF_INET) ? addr.v4.sin_port : addr.v6.sin6_port);

    // convert numeric ip address to ip string
    char tmp[INET6_ADDRSTRLEN] = { 0 };
    if (::inet_ntop(addr.s.sa_family, sin_addr, tmp, sizeof(tmp)) == nullptr)
        return "";

    return fmt::format("{}:{}", tmp, sin_port);
}

bool socket_endpoint::to_string(char* buf_ptr, size_t buf_sz) const
{
    // ip & port
    void* sin_addr = (addr.s.sa_family == AF_INET) ? (void*)&addr.v4.sin_addr : (void*)&addr.v6.sin6_addr;
    int sin_port = ntohs((addr.s.sa_family == AF_INET) ? addr.v4.sin_port : addr.v6.sin6_port);

    // convert numeric ip address to ip string
    char tmp[INET6_ADDRSTRLEN] = { 0 };
    if (::inet_ntop(addr.s.sa_family, sin_addr, tmp, sizeof(tmp)) == nullptr)
    {
        buf_ptr[0] = '\0';
        return false;
    }

    // endpoint string format: ip:port
    ::snprintf(buf_ptr, buf_sz, "%s:%d", tmp, sin_port);
    return true;
}

//
int socket_endpoint::from_udp_address(int protocol_type, const uint8_t* udp_address)
{
    // protocol_type: 1 byte
    int type = (uint8_t)udp_address[0];
    if (type != protocol_type)
        return 0;

    // port: 2 bytes
    uint16_t port = 0;
    ::memcpy(&port, udp_address+1, sizeof(uint16_t));

    // udp v4
    if (protocol_type == SOCKET_TYPE_UDP)
    {
        ::memset(&addr.v4, 0, sizeof(addr.v4));
        addr.s.sa_family = AF_INET;
        addr.v4.sin_port = port;

        // udp addr v4: 4 bytes
        ::memcpy(&addr.v4.sin_addr, udp_address + 1 + sizeof(uint16_t), sizeof(addr.v4.sin_addr));
        return sizeof(addr.v4);
    }
    // udp v6
    else if (protocol_type == SOCKET_TYPE_UDPv6)
    {
        ::memset(&addr.v6, 0, sizeof(addr.v6));
        addr.s.sa_family = AF_INET6;
        addr.v6.sin6_port = port;

        // udp addr v6: 16 bytes
        ::memcpy(&addr.v6.sin6_addr, udp_address + 1 + sizeof(uint16_t), sizeof(addr.v6.sin6_addr));
        return sizeof(addr.v6);
    }

    return 0;
}

/**
 * udp address specs:
 *
 * for UDPv4: 1 + 2 + 4 bytes
 *      1 byte        2 bytes       4 bytes
 * +---------------+----------+----------------+
 * | protocol_type |  port    |  v4.sin_addr   |
 * +---------------+----------+----------------+
 *
 * for UDPv6: 1 + 2 + 16 bytes
 *      1 byte        2 bytes      16 bytes
 * +---------------+----------+----------------+
 * | protocol_type |  port    |  v6.sin_addr   |
 * +---------------+----------+----------------+
 */
int socket_endpoint::to_udp_address(int protocol_type, uint8_t* udp_address) const
{
    // 1 byte
    udp_address[0] = (uint8_t)protocol_type;

    int addr_sz = 1;

    // udp v4
    if (protocol_type == SOCKET_TYPE_UDP)
    {
        // 2 bytes
        ::memcpy(udp_address + addr_sz, &addr.v4.sin_port, sizeof(addr.v4.sin_port));
        addr_sz += sizeof(addr.v4.sin_port);
        // 4 bytes
        ::memcpy(udp_address + addr_sz, &addr.v4.sin_addr, sizeof(addr.v4.sin_addr));
        addr_sz += sizeof(addr.v4.sin_addr);
    }
    // udp v6
    else
    {
        // 2 bytes
        ::memcpy(udp_address + addr_sz, &addr.v6.sin6_port, sizeof(addr.v6.sin6_port));
        addr_sz += sizeof(addr.v6.sin6_port);

        // 16 bytes
        ::memcpy(udp_address + addr_sz, &addr.v6.sin6_addr, sizeof(addr.v6.sin6_addr));
        addr_sz += sizeof(addr.v6.sin6_addr);
    }

    return addr_sz;
}

}
