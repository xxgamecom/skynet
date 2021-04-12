#pragma once

#include "buffer.h"

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

// socket ip proto type
enum socket_type
{
    SOCKET_TYPE_TCP = 0,
    SOCKET_TYPE_UDP = 1,
    SOCKET_TYPE_UDPv6 = 2,
    SOCKET_TYPE_UNKNOWN = 255,
};

// r/w statistics
struct socket_statistics
{
    uint64_t recv_time_ticks = 0;                               // last recv time
    uint64_t send_time_ticks = 0;                               // last send time
    uint64_t recv_bytes = 0;                                    // total recv bytes
    uint64_t send_bytes = 0;                                    // total send bytes
};

// forward declare
struct socket_info;

// socket object
class socket_object final
{
public:
    uint32_t svc_handle = 0;                                    // skynet service handle, the received socket data will be transmitted to the service
    int socket_fd = INVALID_FD;                                 // socket fd
    int socket_id = 0;                                          // socket logic id, used for upper layer
    uint8_t socket_type = SOCKET_TYPE_UNKNOWN;                  // socket ip proto type（TCP/UDP）
    std::atomic<uint8_t> socket_status = SOCKET_STATUS_INVALID; // socket status（read、write、listen...）
    bool reading = false;                                       // half close recv flag
    bool writing = false;                                       // half close send flag
    bool closing = false;                                       // closing flag

    // send
    send_buffer_list send_buffer_list_high;                     // high priority send buffer
    send_buffer_list send_buffer_list_low;                      // low priority send buffer
    int64_t send_buffer_size = 0;                               // wait send data bytes

    std::atomic<uint32_t> sending_count = 0;                    // wait to send count, divide into 2 parts:
                                                                // - high 16 bits: socket id
                                                                // - low 16 bits: actually sending count

    std::atomic<uint16_t> udp_connecting_count = 0;             // udp connecting count

    // statistics
    socket_statistics io_statistics;                            // socket statistics info

    int64_t warn_size = 0;
    
    //
    union
    {
        int size;                                               // recv buffer estimate size
        uint8_t udp_address[UDP_ADDRESS_SIZE];                  //
    } p { 0 };

    // direct send
    std::mutex direct_send_mutex;                               // direct send buffer protected
    int direct_send_offset = 0;                                 // direct send buffer offset
    const void* direct_send_buffer = nullptr;                   // direct send buffer
    size_t direct_send_size = 0;                                // direct send buffer size

public:
    bool is_invalid(int socket_id);

    bool is_send_buffer_empty();
    bool nomore_sending_data();
    bool can_direct_send(int socket_id);

    void shutdown_read();
    void shutdown_write();

    bool is_close_read();
    bool is_close_write();

    //
public:
    void inc_sending_count(int socket_id);
    void dec_sending_count(int socket_id);
    void reset_sending_count(int socket_id);

    void inc_udp_connecting_count();
    void dec_udp_connecting_count();
    void reset_udp_connecting_count();

    // io statistics
public:
    void statistics_recv(int bytes, uint64_t time_ticks);
    void statistics_send(int bytes, uint64_t time_ticks);

public:
    // query socket info
    bool get_socket_info(socket_info& si) const;

};

}

#include "socket_object.inl"
