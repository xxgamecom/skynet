#include "socket.h"
#include "server.h"

#include <cassert>

namespace skynet { namespace socket {


// 发送缓存为空
bool socket::is_send_buffer_empty()
{
    return (wb_list_high.head == nullptr && wb_list_low.head == nullptr);
}

bool socket::has_nomore_sending_data()
{
    return is_send_buffer_empty() &&
            dw_buffer == nullptr && 
            (sending & 0xFFFF) == 0;
}

//
bool socket::can_direct_write(int socket_id)
{
    return this->socket_id == socket_id && 
            has_nomore_sending_data() &&
            status == socket_status::CONNECTED &&
            udp_connecting == 0;
}

void socket::stat_recv(int n, uint64_t time)
{
    stat.recv += n;
    stat.recv_time = time;
}

void socket::stat_send(int n, uint64_t time)
{
    stat.send += n;
    stat.send_time = time;
}

//
void socket::inc_sending_ref(int socket_id)
{
    // only tcp
    if (protocol != protocol_type::TCP)
        return;

    // busy lock
    for (;;)
    {
        uint32_t expect_sending = sending;
        uint16_t expect_socket_id = expect_sending >> 16;

        // inc sending only matching the same socket id
        if (expect_socket_id == server::socket_id_tag16(socket_id))
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
    // notice: udp may inc sending while type == socket_status::ALLOCED
    if (this->socket_id == socket_id && protocol == protocol_type::TCP)
    {
        assert((sending & 0xFFFF) != 0);
        --sending;
    }
}


} }
