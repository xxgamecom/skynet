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
    uint64_t                    recv = 0;                           // total recv bytes (TCP, UDP, BIND) | total accept count (LISTEN)
    uint64_t                    send = 0;                           // total send bytes
    uint64_t                    recv_time = 0;                      // last recv time
    uint64_t                    send_time = 0;                      // last send time
    int64_t                     wb_size = 0;                        // wait send data size
    uint8_t                     reading = 0;                        //
    uint8_t                     writing = 0;                        //

    char                        endpoint[128] = { 0 };              // endpoint info (ip:port)
                                                                    // for LISTEN - it is sock info
                                                                    // for TCP, UDP, BIND - peer info
};


//#define SOCKET_INFO_UNKNOWN 0
//#define SOCKET_INFO_LISTEN 1
//#define SOCKET_INFO_TCP 2
//#define SOCKET_INFO_UDP 3
//#define SOCKET_INFO_BIND 4
//
//#include <stdint.h>
//
//struct socket_info {
//    int id;
//    int type;
//    uint64_t opaque;
//    uint64_t read;
//    uint64_t write;
//    uint64_t rtime;
//    uint64_t wtime;
//    int64_t wbuffer;
//    char name[128];
//    struct socket_info *next;
//};
//
//struct socket_info * socket_info_create(struct socket_info *last);
//void socket_info_release(struct socket_info *);


}
