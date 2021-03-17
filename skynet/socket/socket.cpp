#include "socket.h"
#include "socket_server.h"
#include "socket_info.h"

#include <cassert>

namespace skynet {

//
void socket::inc_sending_ref(int socket_id)
{
    // only tcp
    if (protocol_type != SOCKET_TYPE_TCP)
        return;

    // busy lock
    for (;;)
    {
        uint32_t expect_sending = sending;
        uint16_t expect_socket_id = expect_sending >> 16;

        // inc sending only matching the same socket id
        if (expect_socket_id == socket_pool::socket_id_tag16(socket_id))
        {
            // s->sending maybe overflow, wait socket thread dec. see issue #794
            if ((expect_sending & 0xFFFF) == 0xFFFF)
                continue;

            if (sending.compare_exchange_strong(expect_sending, expect_sending + 1))
                return;

            // here means inc failed, retry
        }
        // socket id changed
        else
        {
            // just return
            return;
        }
    }
}

//
void socket::dec_sending_ref(int socket_id)
{
    // notice: udp may inc sending while status == SOCKET_STATUS_ALLOCED
    if (this->socket_id == socket_id && protocol_type == SOCKET_TYPE_TCP)
    {
        assert((sending & 0xFFFF) != 0);
        --sending;
    }
}

bool socket::get_socket_info(socket_info& si) const
{
    socket_addr sa;
    socklen_t sa_sz = sizeof(sa);
    int closing = 0;

    switch (this->status)
    {
    case SOCKET_STATUS_BIND:
    {
        si.type = SOCKET_INFO_TYPE_BIND;
        si.endpoint[0] = '\0';
    }
        break;
    case SOCKET_STATUS_LISTEN:
    {
        si.type = SOCKET_INFO_TYPE_LISTEN;
        // local server listen address
        if (::getsockname(this->socket_fd, &sa.addr.s, &sa_sz) == 0)
            sa.to_string(si.endpoint, sizeof(si.endpoint));
    }
        break;
    case SOCKET_STATUS_HALF_CLOSE_READ:
    case SOCKET_STATUS_HALF_CLOSE_WRITE:
        closing = 1;
    case SOCKET_STATUS_CONNECTED:
        if (this->protocol_type == SOCKET_TYPE_TCP)
        {
            si.type = closing ? SOCKET_INFO_TYPE_CLOSING : SOCKET_INFO_TYPE_TCP;
            // remote client address
            if (::getpeername(this->socket_fd, &sa.addr.s, &sa_sz) == 0)
                sa.to_string(si.endpoint, sizeof(si.endpoint));
        }
        else
        {
            si.type = SOCKET_INFO_TYPE_UDP;
            //
            if (sa.from_udp_address(protocol_type, p.udp_address) != 0)
            {
                sa.to_string(si.endpoint, sizeof(si.endpoint));
            }
        }
        break;
    default:
        return false;
    }

    // base info
    si.socket_id = this->socket_id;
    si.svc_handle = this->svc_handle;

    // send/recv statistics info
    si.recv = this->stat.recv;
    si.send = this->stat.send;
    si.recv_time = this->stat.recv_time;
    si.send_time = this->stat.send_time;
    si.reading = this->reading;
    si.writing = this->writing;

    // write buffer size
    si.wb_size = this->wb_size;

    return true;
}

}
