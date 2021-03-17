#include "socket.h"
#include "socket_server.h"

#include <cassert>

namespace skynet {

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
        if (expect_socket_id == socket_server::socket_id_tag16(socket_id))
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
    // notice: udp may inc sending while type == SOCKET_STATUS_ALLOCED
    if (this->socket_id == socket_id && protocol == protocol_type::TCP)
    {
        assert((sending & 0xFFFF) != 0);
        --sending;
    }
}

}
