#pragma once

#include <stdint.h>

namespace skynet { namespace socket {

// server内的socket信息
class socket_info final
{
public:
    // socket状态
    enum status
    {
        UNKNOWN                 = 0,                        // 未知状态
        LISTEN                  = 1,                        // 监听状态
        TCP                     = 2,                        // 已建立 tcp 连接
        UDP                     = 3,                        // 已联机 udp 连接
        BIND                    = 4,                        // 绑定状态
    };

public:
    // 基本信息
    int                         socket_id = 0;              //
    int                         status = status::UNKNOWN;   // socket状态
    uint64_t                    svc_handle = 0;             // skynet服务句柄
    
    // 收/发 统计信息
    uint64_t                    recv = 0;                   // 总收字节数
    uint64_t                    send = 0;                   // 总发字节数
    uint64_t                    recv_time = 0;              // 最近收数据时间
    uint64_t                    send_time = 0;              // 最近发数据时间
    
    //
    int64_t                     wb_size = 0;                // 待发数据大小
    char                        endpoint[128] = { 0 };      // 端点信息 (endpoint = ip:port)
    
    socket_info*                next = nullptr;             // 链表指针

public:
    /**
     * 创建socket_info
     * @param last_si 链表的最后一个节点
     */
    static socket_info* create(socket_info* last_si);

    /**
     * 清理socket_info (释放包括给定链节点在内的后续的节点)
     * @param si 需要清理的socket_info
     */
    static void release(socket_info* si);

};

} }
