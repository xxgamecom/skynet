#pragma once

namespace skynet { namespace network {

//------------------------------------------------------------------------------
// socket options
// asio将socket分为tcp和udp等, 需要统一处理socket选项
//------------------------------------------------------------------------------
enum sock_options
{
    SOCK_OPT_RECV_BUFFER = 0,                   // socket recv buffer size
    SOCK_OPT_SEND_BUFFER = 1,                   // socket send buffer size
    SOCK_OPT_KEEPALIVE = 2,                     // socket keep-alive option
    SOCK_OPT_NODELAY = 3,                       // socket nagle algorithm option
    SOCK_OPT_LINGER = 4,                        // socket linger option (unit: second)
    SOCK_OPT_BROADCAST = 5,                     // socket broadcast option
};


}}
