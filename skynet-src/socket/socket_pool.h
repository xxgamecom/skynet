#pragma once

#include "socket.h"
#include "socket_info.h"

#include <list>
#include <atomic>
#include <array>

namespace skynet {

// socket session pool
class socket_pool final
{
public:
    // constants
    enum
    {
        MAX_SOCKET_P                    = 16,                               // max number of socket (power of 2)
        MAX_SOCKET                      = 1 << MAX_SOCKET_P,                // MAX_SOCKET = 2^MAX_SOCKET_P = (0xFFFF)
    };

private:
    std::array<socket, MAX_SOCKET>      socket_array_;                      // socket列表
    std::atomic<int>                    alloc_socket_id_ { 0 };             // 用于分配socket id

public:
    // 从socket池中分配一个socket, 返回一个socket id
    int alloc_socket_id();

    //
    socket& get_socket(int socket_id);
    //
    std::array<socket, MAX_SOCKET>& get_all_sockets();
    // 查询所有socket信息 (socket_info是一个链表)
    void get_socket_info(std::list<socket_info>& si_list);

public:
    // 计算socket slot数组下标
    static uint32_t calc_slot_index(int socket_id);
    // 高16位
    static uint16_t socket_id_tag16(int socket_id);

};

}

#include "socket_pool.inl"
