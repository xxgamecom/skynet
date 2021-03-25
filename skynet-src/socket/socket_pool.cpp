#include "socket_pool.h"

namespace skynet {

int socket_pool::alloc_socket_id()
{
    for (int i = 0; i < MAX_SOCKET; i++)
    {
        int socket_id = ++alloc_socket_id_;

        // overflow
        if (socket_id < 0)
        {
            alloc_socket_id_ &= 0x7FFFFFFF;
            socket_id = alloc_socket_id_;
        }

        //
        auto& socket_ref = get_socket(socket_id);

        // socket is not available
        if (socket_ref.status != SOCKET_STATUS_INVALID)
            continue;

        // set socket status: alloced
        uint8_t expect_status = socket_ref.status;
        if (expect_status == SOCKET_STATUS_INVALID)
        {
            if (socket_ref.status.compare_exchange_strong(expect_status, SOCKET_STATUS_ALLOCED))
            {
                socket_ref.socket_id = socket_id;
                socket_ref.protocol_type = SOCKET_TYPE_UNKNOWN;
                socket_ref.udp_connecting = 0;  // socket_server::udp_connect 可以直接增加 socket_ref.udp_conncting (在其他线程, new_fd之前), 因此这里重置为0
                socket_ref.socket_fd = INVALID_FD;
                return socket_id;
            }
                // socket status change before set, retry
            else
            {
                --i;
            }
        }
    }

    return -1;
}

void socket_pool::get_socket_info(std::list<socket_info>& si_list)
{
    // reset
    si_list.clear();

    //
    for (auto& socket_ref : socket_array_)
    {
        auto socket_id = socket_ref.socket_id;
        socket_info si;

        // get_socket_info() may call in different thread, so check socket id again
        if (socket_ref.get_socket_info(si) && socket_ref.socket_id == socket_id)
        {
            si_list.push_back(si);
        }
    }
}

}

