#pragma once

#include "socket_object.h"
#include "socket_info.h"

#include <list>
#include <atomic>
#include <array>

namespace skynet {

/**
 * socket object pool
 *
 * specs:
 * - max socket:
 * - socket id:
 */
class socket_object_pool final
{
public:
    // constants
    enum
    {
        MAX_SOCKET_P = 16,                                  // max number of socket (power of 2)
        MAX_SOCKET = 1 << MAX_SOCKET_P,                     // MAX_SOCKET = 2^MAX_SOCKET_P = (0xFFFF)
    };

private:
    std::array<socket_object, MAX_SOCKET> socket_array_;    // socket array
    std::atomic<int> alloc_socket_id_ { 0 };

public:
    /**
     * alloc socket and return a new socket id
     *
     * @return socket id
     */
    int alloc_socket();
    /**
     * used end, put back to pool
     *
     * @param socket_id
     * @return
     */
    void free_socket(int socket_id);

    /**
     * get socket object ref by socket id
     *
     * @param socket_id
     * @return socket object reference
     */
    socket_object& get_socket(int socket_id);

    /**
     * get all socket object
     *
     * @return socket object array
     */
    std::array<socket_object, MAX_SOCKET>& get_all_sockets();

    /**
     * get all socket object info
     *
     * @param si_list link list
     */
    void get_socket_info(std::list<socket_info>& si_list);

public:
    // socket array index
    static uint32_t calc_slot_index(int socket_id);
    // socket id high 16 bits
    static uint16_t socket_id_high(int socket_id);

};

}

#include "socket_object_pool.inl"
