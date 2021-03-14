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

namespace skynet { namespace socket {

//----------------------------------------------
// common constants
//----------------------------------------------

// 无效fd
#define INVALID_FD              -1

// udp
#define UDP_ADDRESS_SIZE        19                                          // udp地址长度 = ipv6 128bit + port 16bit + 1 byte type

//----------------------------------------------
// 
//----------------------------------------------

// socket事件类型
enum socket_event
{
    SOCKET_DATA                 = 0,                                        // socket 正常数据
    SOCKET_CLOSE                = 1,                                        // socket close 数据
    SOCKET_OPEN                 = 2,                                        // socket open 数据 (connect)
    SOCKET_ACCEPT               = 3,                                        // socket accept 数据
    SOCKET_ERROR                = 4,                                        // socket error 数据
    SOCKET_EXIT                 = 5,                                        // socket exit 数据
    SOCKET_UDP                  = 6,                                        // socket udp 数据
    SOCKET_WARNING              = 7,                                        // socket warning 数据
};

// 协议类型
enum protocol_type
{
    TCP                         = 0,                                        //
    UDP                         = 1,                                        //
    UDPv6                       = 2,                                        //
    UNKNOWN                     = 255,                                      //
};

// 发送缓存
struct write_buffer
{
    write_buffer*               next = nullptr;                             // 

    const void*                 buffer = nullptr;                           //
    char*                       ptr = nullptr;                              //
    size_t                      sz = 0;                                     //
    bool                        is_userobject = false;                      //
    uint8_t                     udp_address[UDP_ADDRESS_SIZE] = { 0 };      //
};

// 发送缓存队列
struct write_buffer_list
{
    write_buffer*               head = nullptr;                             // 写缓冲区的头指针
    write_buffer*               tail = nullptr;                             // 写缓冲区的尾指针
};

// 
struct socket_object_interface
{
    const void* (*buffer)(const void*);
    size_t (*size)(const void*);
    void (*free)(void*);
};

//
struct socket_message
{
    int                         socket_id = 0;                  // 
    uint64_t                    svc_handle = 0;                 // skynet service handle, 收到的网络数据最终会传送给服务
    int                         ud = 0;                         // for accept: ud is new connection's fd; for data: ud is the size of data
    char*                       data = nullptr;                 // data ptr
};

} }
