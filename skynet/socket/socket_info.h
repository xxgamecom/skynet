#pragma once

#include <cstdint>

namespace skynet { namespace socket {

// socket info type
enum class socket_info_type : int
{
    UNKNOWN                     = 0,                                // type - unknown
    LISTEN                      = 1,                                // type - listen
    TCP                         = 2,                                // type - tcp connected
    UDP                         = 3,                                // type - udp connected
    BIND                        = 4,                                // type - bind
};

// socket info in socket_server
struct socket_info
{
    // base
    int                         socket_id = 0;                      //
    socket_info_type            type = socket_info_type::UNKNOWN;   // socket info type (int)
    uint64_t                    svc_handle = 0;                     // skynet service handle
    
    // recv/send statistics
    uint64_t                    recv = 0;                           // total recv bytes (TCP, UDP, BIND) | total accept count (LISTEN)
    uint64_t                    send = 0;                           // total send bytes
    uint64_t                    recv_time = 0;                      // last recv time
    uint64_t                    send_time = 0;                      // last send time
    
    //
    int64_t                     wb_size = 0;                        // wait send data size

    char                        endpoint[128] = { 0 };              // endpoint info (ip:port)
                                                                    // for LISTEN - it is sock info
                                                                    // for TCP, UDP, BIND - peer info
};

} }
