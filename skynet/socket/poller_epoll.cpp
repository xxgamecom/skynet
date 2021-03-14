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

namespace skynet { namespace socket {

//  初始化
bool poller::init()
{
    poll_fd_ = ::epoll_create(1024);
    return (poll_fd_ != INVALID_FD);
}

bool poller::add(int sock, void* ud)
{
    epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.ptr = ud;
    return !(::epoll_ctl(poll_fd_, EPOLL_CTL_ADD, sock, &ev) == -1);
}

void poller::del(int sock)
{
    ::epoll_ctl(poll_fd_, EPOLL_CTL_DEL, sock , nullptr);
}

void poller::write(int sock, void* ud, bool enable_write)
{
    epoll_event ev;
    ev.events = EPOLLIN | (enable_write ? EPOLLOUT : 0);
    ev.data.ptr = ud;
    ::epoll_ctl(poll_fd_, EPOLL_CTL_MOD, sock, &ev);
}

int poller::wait(event* event_ptr, int max_events/* = MAX_EVENT*/)
{
    epoll_event ev[max_events];
    int n = ::epoll_wait(poll_fd_ , ev, max_events, -1);
    for (int i = 0; i < n; i++)
    {
        event_ptr[i].socket_ptr = (socket*)ev[i].data.ptr;
        uint32_t flag = ev[i].events;
        event_ptr[i].is_write = (flag & EPOLLOUT) != 0;
        event_ptr[i].is_read = (flag & (EPOLLIN | EPOLLHUP)) != 0;
        event_ptr[i].is_error = (flag & EPOLLERR) != 0;
        event_ptr[i].is_eof = false;
    }

    return n;
}

} }

#endif

