#include "udp_session.h"

namespace skynet::net::impl {

// session id
void udp_session_impl::session_id(session_id_t id)
{

}

session_id_t udp_session_impl::session_id()
{
    return INVALID_SESSION_ID;
}

void udp_session_impl::check_idle(idle_type check_type, int32_t check_seconds)
{

}

// r/w statistics
int64_t udp_session_impl::read_bytes()
{
    return 0;
}

int64_t udp_session_impl::write_bytes()
{
    return 0;
}

// r/w delta statistics (note: will reset after called)
int64_t udp_session_impl::delta_read_bytes()
{
    return 0;
}

int64_t udp_session_impl::delta_write_bytes()
{
    return 0;
}

}