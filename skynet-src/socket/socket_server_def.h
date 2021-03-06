#pragma once

#include <atomic>
#include <mutex>

#include <unistd.h>
#include <arpa/inet.h>

// EAGAIN and EWOULDBLOCK may be not the same value.
#if (EAGAIN != EWOULDBLOCK)
#   define AGAIN_WOULDBLOCK EAGAIN : case EWOULDBLOCK
#else
#   define AGAIN_WOULDBLOCK EAGAIN
#endif

namespace skynet {

//----------------------------------------------
// common constants
//----------------------------------------------

//
#define INVALID_FD              -1
#define INVALID_SOCKET_ID       -1

// udp地址长度 = ipv6 128bit + port 16bit + 1 byte type
#define UDP_ADDRESS_SIZE        19

//----------------------------------------------
// 
//----------------------------------------------

// socket event type
enum socket_event
{
    SOCKET_EVENT_DATA = 0,              // socket data event
    SOCKET_EVENT_CLOSE = 1,             // socket close event
    SOCKET_EVENT_OPEN = 2,              // socket connect event
    SOCKET_EVENT_ACCEPT = 3,            // socket accept event
    SOCKET_EVENT_ERROR = 4,             // socket error event
    SOCKET_EVENT_EXIT = 5,              // socket exit event
    SOCKET_EVENT_UDP = 6,               // socket udp event
    SOCKET_EVENT_WARNING = 7,           // socket warning event
    SOCKET_EVENT_RST = 8,               // only for internal use
};

//
struct socket_message
{
    int socket_id = 0;                  //
    uint32_t svc_handle = 0;            // skynet service handle, where message sendding, TODO: change to uint32_t
    int ud = 0;                         // for accept: ud is new connection's fd; for data: ud is the size of data
    char* data_ptr = nullptr;           // data ptr
};

}
