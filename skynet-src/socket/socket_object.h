#pragma once

#include "socket_server_def.h"

#include <cstdint>
#include <atomic>
#include <mutex>

namespace skynet {

/**
 * socket status
 *
 * - for acceptor
 *   INVALID -> ALLOCED -> PREPARE_LISTEN -> LISTEN ->
 *
 * - for accept client connection
 *   INVALID -> ALLOCED -> PREPARE_ACCEPT -> CONNECTED ->
 *
 * - for connect remote
 *   INVALID -> ALLOCED -> CONNECTING -> CONNECTED ->
 */
enum socket_status
{
    SOCKET_STATUS_INVALID = 0,                                  // socket is free (invalid), wait alloced
    SOCKET_STATUS_ALLOCED = 1,                                  // socket has been alloced
    SOCKET_STATUS_PREPARE_LISTEN = 2,                           // prepare listen (only for acceptor)
    SOCKET_STATUS_LISTEN = 3,                                   // listen, can accept client connection (only for acceptor)
    SOCKET_STATUS_CONNECTING = 4,                               // 正在连接 (connect失败时状态, tcp会尝试重新connect)
    SOCKET_STATUS_CONNECTED = 5,                                // connected, can recv/send data
    SOCKET_STATUS_HALF_CLOSE_READ = 6,                          //
    SOCKET_STATUS_HALF_CLOSE_WRITE = 7,                         //
    SOCKET_STATUS_PREPARE_ACCEPT = 8,                           // prepare accept (only for server accept client connection)
    SOCKET_STATUS_BIND = 9,                                     // bind os fd (stdin, stdout)
};

// 协议类型
enum socket_type
{
    SOCKET_TYPE_TCP = 0,
    SOCKET_TYPE_UDP = 1,
    SOCKET_TYPE_UDPv6 = 2,
    SOCKET_TYPE_UNKNOWN = 255,
};

// 发送缓存
struct write_buffer
{
    write_buffer* next = nullptr;                               //

    const void* buffer = nullptr;                               //
    char* ptr = nullptr;                                        //
    size_t sz = 0;                                              //
    bool is_user_object = false;                                //
    uint8_t udp_address[UDP_ADDRESS_SIZE] = { 0 };              //
};

// 发送缓存队列
struct write_buffer_list
{
    write_buffer* head = nullptr;                               // 写缓冲区的头指针
    write_buffer* tail = nullptr;                               // 写缓冲区的尾指针
};

// forward declare
struct socket_info;

// socket object
class socket_object final
{
public:
    // r/w statistics
    struct socket_statistics
    {
        uint64_t recv_time_ticks = 0;                           // last recv time
        uint64_t send_time_ticks = 0;                           // last send time
        uint64_t recv_bytes = 0;                                // total recv bytes
        uint64_t send_bytes = 0;                                // total send bytes
    };

public:
    uint32_t svc_handle = 0;                                    // skynet service handle
                                                                // the received socket data will be transmitted to the service.

    write_buffer_list wb_list_high;                             // high priority write buffer
    write_buffer_list wb_list_low;                              // low priority write buffer
    int64_t wb_size = 0;                                        // wait send data size

    socket_statistics io_statistics;                            // socket statistics info

    std::atomic<uint32_t> sending = 0;                          // divide into 2 parts:
                                                                // - high 16 bits: socket id
                                                                // - low 16 bits: actually sending count

    int socket_fd = INVALID_FD;                                 // socket fd
    int socket_id = 0;                                          // 应用层维护一个与fd对应的socket id
    uint8_t protocol_type = SOCKET_TYPE_UNKNOWN;                // socket protocol type（TCP/UDP）
    std::atomic<uint8_t> status = SOCKET_STATUS_INVALID;        // socket status（read、write、listen...）
    bool reading = false;                                       // half close read flag
    bool writing = false;                                       // half close write flag
    bool closing = false;                                       // closing flag

    std::atomic<uint16_t> udp_connecting = 0;
    int64_t warn_size = 0;
    
    //
    union
    {
        int size;                                               // 读缓存预估需要的大小
        uint8_t udp_address[UDP_ADDRESS_SIZE];                  //
    } p { 0 };

    //
    std::mutex dw_mutex;                                        // 发送缓存保护
    int dw_offset = 0;                                          // 立刻发送缓冲区偏移
    const void* dw_buffer = nullptr;                            // 立刻发送缓冲区
    size_t dw_size = 0;                                         // 立刻发送缓冲区大小

public:
    bool is_invalid(int socket_id);

    // 发送缓存为空
    bool is_send_buffer_empty();
    // 没有发送数据
    bool nomore_sending_data();
    //
    bool can_direct_write(int socket_id);

    void shutdown_read();
    void shutdown_write();

    bool is_close_read();
    bool is_close_write();

    //
public:
    //
    void inc_sending_ref(int socket_id);
    //
    void dec_sending_ref(int socket_id);

    // io statistics
public:
    // recv statistics
    void stat_recv(int bytes, uint64_t time_ticks);
    // send statistics
    void stat_send(int bytes, uint64_t time_ticks);

public:
    // query socket info
    bool get_socket_info(socket_info& si) const;

};

}

#include "socket_object.inl"
