#include "socket_object.h"
#include "socket_server.h"
#include "socket_info.h"

#include "utils/socket_helper.h"

#include <cassert>

namespace skynet {

//
void socket_object::inc_sending_count(int socket_id)
{
    // only tcp
    if (socket_type != SOCKET_TYPE_TCP)
        return;

    // busy lock
    for (;;)
    {
        uint32_t expect_sending_count = sending_count;
        uint16_t expect_socket_id = expect_sending_count >> 16;

        // inc sending only matching the same socket id
        if (expect_socket_id == socket_object_pool::socket_id_high16(socket_id))
        {
            // s->sending maybe overflow, wait socket thread dec.
            if ((expect_sending_count & 0xFFFF) == 0xFFFF)
                continue;

            if (sending_count.compare_exchange_strong(expect_sending_count, expect_sending_count + 1))
                return;

            // to here means inc failed, retry
        }
        else
        {
            // to here means socket id changed, just return
            return;
        }
    }
}

//
void socket_object::dec_sending_count(int socket_id)
{
    // notice: udp may inc sending while status == SOCKET_STATUS_ALLOCED
    if (this->socket_id == socket_id && socket_type == SOCKET_TYPE_TCP)
    {
        assert((sending_count & 0xFFFF) != 0);
        --sending_count;
    }
}

void socket_object::reset_sending_count(int socket_id)
{
    sending_count = socket_object_pool::socket_id_high16(socket_id) << 16 | 0;
}

void socket_object::inc_udp_connecting_count()
{
    ++udp_connecting_count;
}

void socket_object::dec_udp_connecting_count()
{
    --udp_connecting_count;
}

void socket_object::reset_udp_connecting_count()
{
    udp_connecting_count = 0;
}

bool socket_object::get_socket_info(socket_info& si) const
{
    socket_endpoint endpoint;
    socklen_t endpoint_sz = sizeof(endpoint);
    bool closing = false;

    switch (socket_status)
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
        if (::getsockname(this->socket_fd, &endpoint.addr.s, &endpoint_sz) == 0)
            endpoint.to_string(si.endpoint, sizeof(si.endpoint));
    }
        break;
    case SOCKET_STATUS_HALF_CLOSE_READ:
    case SOCKET_STATUS_HALF_CLOSE_WRITE:
        closing = true;
    case SOCKET_STATUS_CONNECTED:
        if (this->socket_type == SOCKET_TYPE_TCP)
        {
            si.type = closing ? SOCKET_INFO_TYPE_CLOSING : SOCKET_INFO_TYPE_TCP;
            // remote client address
            if (::getpeername(this->socket_fd, &endpoint.addr.s, &endpoint_sz) == 0)
                endpoint.to_string(si.endpoint, sizeof(si.endpoint));
        }
        else
        {
            si.type = SOCKET_INFO_TYPE_UDP;
            //
            if (endpoint.from_udp_address(this->socket_type, p.udp_address) != 0)
            {
                endpoint.to_string(si.endpoint, sizeof(si.endpoint));
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
    si.recv_bytes = this->io_statistics.recv_bytes;
    si.send_bytes = this->io_statistics.send_bytes;
    si.recv_time_ticks = this->io_statistics.recv_time_ticks;
    si.send_time_ticks = this->io_statistics.send_time_ticks;
    si.reading = this->reading;
    si.writing = this->writing;

    // write buffer size
    si.wb_size = this->wb_size;

    return true;
}

}
