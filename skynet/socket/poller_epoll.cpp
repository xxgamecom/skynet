#include "poller.h"
#include "socket.h"

// linux epoll
#ifdef __linux__

#include <netdb.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>

namespace skynet {

//  初始化
bool poller::init()
{
    poll_fd_ = ::epoll_create(1024);
    return (poll_fd_ != INVALID_FD);
}

bool poller::add(int socket_fd, void* ud)
{
    epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.ptr = ud;
    return !(::epoll_ctl(poll_fd_, EPOLL_CTL_ADD, socket_fd, &ev) == -1);
}

void poller::del(int socket_fd)
{
    ::epoll_ctl(poll_fd_, EPOLL_CTL_DEL, socket_fd , nullptr);
}

int poller::enable(int socket_fd, void* ud, bool enable_read, bool enable_write)
{
    epoll_event ev;
    ev.events = (enable_read ? EPOLLIN : 0) | (enable_write ? EPOLLOUT : 0);
    ev.data.ptr = ud;
    if (::epoll_ctl(poll_fd_, EPOLL_CTL_MOD, socket_fd, &ev) == -1)
    {
        return 1;
    }

    return 0;
}

int poller::wait(event* event_ptr, int max_events/* = MAX_WAIT_EVENT*/)
{
    epoll_event ev[max_events];
    int n = ::epoll_wait(poll_fd_ , ev, max_events, -1);
    for (int i = 0; i < n; i++)
    {
        event_ptr[i].socket_ptr = (socket*)ev[i].data.ptr;
        uint32_t flag = ev[i].events;
        event_ptr[i].is_writeable = (flag & EPOLLOUT) != 0;
        event_ptr[i].is_readable = (flag & EPOLLIN) != 0;
        event_ptr[i].is_error = (flag & EPOLLERR) != 0;
        event_ptr[i].is_eof = (flag & EPOLLHUP) != 0;

    }

    return n;
}

}

#endif

