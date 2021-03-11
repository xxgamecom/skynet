#include "socket_info.h"

namespace skynet { namespace socket {

socket_info* socket_info::create(socket_info* last_si)
{
    socket_info* si = new socket_info;
    si->next = last_si;
    return si;
}

void socket_info::release(socket_info* si)
{
    while (si != nullptr)
    {
        socket_info* tmp = si;
        si = si->next;
        delete tmp;
    }
}

} }
