#include "socket_object_pool.h"

namespace skynet {

int socket_object_pool::alloc_socket()
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
        if (socket_ref.socket_status != SOCKET_STATUS_INVALID)
            continue;

        // set socket status: alloced
        uint8_t expect_status = socket_ref.socket_status;
        if (expect_status == SOCKET_STATUS_INVALID)
        {
            if (socket_ref.socket_status.compare_exchange_strong(expect_status, SOCKET_STATUS_ALLOCED))
            {
                socket_ref.socket_id = socket_id;
                socket_ref.socket_type = SOCKET_TYPE_UNKNOWN;
                socket_ref.reset_udp_connecting_count();
                socket_ref.socket_fd = INVALID_FD;
                return socket_id;
            }
            else
            {
                // socket status change before set, retry
                --i;
            }
        }
    }

    return -1;
}

void socket_object_pool::free_socket(int socket_id)
{
    int idx = socket_array_index(socket_id);
    socket_array_[idx].socket_status = SOCKET_STATUS_INVALID;
}

void socket_object_pool::get_socket_info(std::list<socket_info>& si_list)
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

