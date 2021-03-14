#pragma once

#include "socket_server_def.h"

#include <cstdint>
#include <atomic>
#include <mutex>

namespace skynet { namespace socket {


// socket状态
enum socket_status
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

// socket信息
class socket final
{
public:
    // socket状态统计
    struct socket_stat
    {
        uint64_t                    recv_time = 0;                      // 读数据时间
        uint64_t                    send_time = 0;                      // 写数据时间
        uint64_t                    recv = 0;                           // 读字节数
        uint64_t                    send = 0;                           // 写字节数
    };

public:
    uint64_t                        svc_handle = 0;                     // 所属skynet服务句柄, 收到的网络数据最终会传送给服务

    write_buffer_list               wb_list_high;                       // 高优先级写队列
    write_buffer_list               wb_list_low;                        // 低优先级写队列
    int64_t                         wb_size = 0;                        // 待发数据大小

    socket_stat                     stat;                               // socket send/recv statistics info
    
    std::atomic<uint32_t>           sending { 0 };                      // divide into 2 parts, high 16 bits: socket id, low 16 bits: actually sending count

    int                             socket_fd = INVALID_FD;             // socket fd
    int                             socket_id = 0;                      // 应用层维护一个与fd对应的socket id
    uint8_t                         protocol { protocol_type::UNKNOWN}; // socket的协议类型（TCP/UDP）
    std::atomic<uint8_t>            status { socket_status::FREE };     // socket的状态（读、写、监听等）
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
    bool has_nomore_sending_data();
    //
    bool can_direct_write(int socket_id);

    // stat
public:
    // 收统计
    void stat_recv(int n, uint64_t time);
    // 发统计
    void stat_send(int n, uint64_t time);

    //
public:
    //
    void inc_sending_ref(int socket_id);
    //
    void dec_sending_ref(int socket_id);

};

} }
