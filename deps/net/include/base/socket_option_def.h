#pragma once

namespace skynet::net {

/**
 * socket options
 * asio divides socket into tcp, udp and so on, unified.
 */
enum sock_options
{
    SOCK_OPT_RECV_BUFFER = 0,                   // socket recv buffer size
    SOCK_OPT_SEND_BUFFER = 1,                   // socket send buffer size
    SOCK_OPT_KEEPALIVE = 2,                     // socket keep-alive option
    SOCK_OPT_NODELAY = 3,                       // socket nagle algorithm option
    SOCK_OPT_LINGER = 4,                        // socket linger option (unit: second)
    SOCK_OPT_BROADCAST = 5,                     // socket broadcast option
};

}
