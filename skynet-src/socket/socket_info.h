#pragma once

#include <cstdint>

namespace skynet {

// socket info type
enum socket_info_type
{
    SOCKET_INFO_TYPE_UNKNOWN    = 0,                                // type - unknown
    SOCKET_INFO_TYPE_LISTEN     = 1,                                // type - listen
    SOCKET_INFO_TYPE_TCP        = 2,                                // type - tcp connected
    SOCKET_INFO_TYPE_UDP        = 3,                                // type - udp connected
    SOCKET_INFO_TYPE_BIND       = 4,                                // type - bind
    SOCKET_INFO_TYPE_CLOSING    = 5,                                // type - closing
};

// socket info in socket_server
struct socket_info
{
    // base
    int                         socket_id = 0;                      //
    int                         type = SOCKET_INFO_TYPE_UNKNOWN;    // socket info type
    uint64_t                    svc_handle = 0;                     // skynet service handle
    
    // recv/send statistics
    uint64_t                    recv_bytes = 0;                     // total recv bytes (TCP, UDP, BIND) | total accept count (LISTEN)
    uint64_t                    send_bytes = 0;                     // total send bytes
    uint64_t                    recv_time_ticks = 0;                // last recv time
    uint64_t                    send_time_ticks = 0;                // last send time
    int64_t                     wb_size = 0;                        // wait send data size
    uint8_t                     reading = 0;                        //
    uint8_t                     writing = 0;                        //

    char                        endpoint[128] = { 0 };              // endpoint info (ip:port)
                                                                    // for LISTEN - it is sock info
                                                                    // for TCP, UDP, BIND - peer info
};

}
