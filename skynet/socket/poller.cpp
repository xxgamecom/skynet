#include "poller.h"
#include "socket_server_def.h"

#include <unistd.h>

namespace skynet { namespace socket {

poller::~poller()
{
    fini();
}

// 清理
void poller::fini()
{
    ::close(poll_fd_);
    poll_fd_ = INVALID_FD;
}

bool poller::is_invalid()
{
    return poll_fd_ == INVALID_FD;
}

} }
