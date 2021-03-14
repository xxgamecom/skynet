#pragma once

#include "socket_server_def.h"

#include <cstdint>
#include <atomic>
#include <mutex>

namespace skynet { namespace socket {

// socket信息
class socket final
{
public:
    // socket recv/send statistics
    struct statistics
    {
        uint64_t                    recv_time = 0;                      // last recv time
        uint64_t                    send_time = 0;                      // last send time
        uint64_t                    recv = 0;                           // total recv bytes
        uint64_t                    send = 0;                           // total send bytes
    };

    // socket status
    enum status
    {
        FREE                            = 0,                                // 空闲, 可被分配
        ALLOCED                         = 1,                                // 被分配
        PLISTEN                         = 2,                                // 等待监听 (监听套接字拥有)
        LISTEN                          = 3,                                // 监听, 可接受客户端的连接 (监听套接字才拥有)
        CONNECTING                      = 4,                                // 正在连接 (connect失败时状态, tcp会尝试重新connect)
        CONNECTED                       = 5,                                // 已连接, 可以收发数据
        HALF_CLOSE                      = 6,                                //
        PACCEPT                         = 7,                                // 等待连接（连接套接字才拥有）
        BIND                            = 8,                                // 绑定阶段
    };

public:
    uint64_t                        svc_handle = 0;                     // skynet service handle
                                                                        // the received socket data will be transmitted to the service.

    write_buffer_list               wb_list_high;                       // high priority write buffer
    write_buffer_list               wb_list_low;                        // low priority write buffer
    int64_t                         wb_size = 0;                        // wait send data size

    statistics                      stat;                               // socket send/recv statistics info
    
    std::atomic<uint32_t>           sending { 0 };                      // divide into 2 parts:
                                                                        // - high 16 bits: socket id
                                                                        // - low 16 bits: actually sending count

    int                             socket_fd = INVALID_FD;             // socket fd
    int                             socket_id = 0;                      // 应用层维护一个与fd对应的socket id
    uint8_t                         protocol { protocol_type::UNKNOWN}; // socket的协议类型（TCP/UDP）
    std::atomic<uint8_t>            status { status::FREE };            // socket status（read、write、listen...）
    std::atomic<uint16_t>           udp_connecting { 0 };               //
    int64_t                         warn_size = 0;                      //
    
    //
    union
    {
        int                         size;                               // 读缓存预估需要的大小
        uint8_t                     udp_address[UDP_ADDRESS_SIZE];      //
    } p { 0 };
    
    //
    std::mutex                      dw_mutex;                           // 发送缓存保护
    int                             dw_offset = 0;                      // 立刻发送缓冲区偏移
    const void*                     dw_buffer = nullptr;                // 立刻发送缓冲区
    size_t                          dw_size = 0;                        // 立刻发送缓冲区大小

public:
    // 发送缓存为空
    bool is_send_buffer_empty();
    // 没有发送数据
    bool nomore_sending_data();
    //
    bool can_direct_write(int socket_id);

    // stat
public:
    // recv statistics
    void stat_recv(int n, uint64_t time);
    // send statistics
    void stat_send(int n, uint64_t time);

    //
public:
    //
    void inc_sending_ref(int socket_id);
    //
    void dec_sending_ref(int socket_id);

};

} }

#include "socket.inl"
