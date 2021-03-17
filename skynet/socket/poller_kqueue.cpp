#include "poller.h"
#include "socket.h"

// mac, freebsd
#if defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__) || defined (__NetBSD__)

#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/event.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

namespace skynet {

bool poller::init()
{
    poll_fd_ = ::kqueue();
    return (poll_fd_ != INVALID_FD);
}

bool poller::add(int sock_fd, void* ud)
{
    struct kevent ke;

    //
    EV_SET(&ke, sock_fd, EVFILT_READ, EV_ADD, 0, 0, ud);
    if (::kevent(poll_fd_, &ke, 1, nullptr, 0, nullptr) == -1 ||	ke.flags & EV_ERROR)
    {
        return false;
    }
    
    EV_SET(&ke, sock_fd, EVFILT_WRITE, EV_ADD, 0, 0, ud);
    if (::kevent(poll_fd_, &ke, 1, nullptr, 0, nullptr) == -1 ||	ke.flags & EV_ERROR)
    {
        EV_SET(&ke, sock_fd, EVFILT_READ, EV_DELETE, 0, 0, nullptr);
        ::kevent(poll_fd_, &ke, 1, nullptr, 0, nullptr);
        return false;
    }
    
    EV_SET(&ke, sock_fd, EVFILT_WRITE, EV_DISABLE, 0, 0, ud);
    if (::kevent(poll_fd_, &ke, 1, nullptr, 0, nullptr) == -1 ||	ke.flags & EV_ERROR)
    {
        del(sock_fd);
        return false;
    }
    
    return true;
}

void poller::del(int sock_fd)
{
    struct kevent ke;

    EV_SET(&ke, sock_fd, EVFILT_READ, EV_DELETE, 0, 0, nullptr);
    ::kevent(poll_fd_, &ke, 1, nullptr, 0, nullptr);
    
    EV_SET(&ke, sock_fd, EVFILT_WRITE, EV_DELETE, 0, 0, nullptr);
    ::kevent(poll_fd_, &ke, 1, nullptr, 0, nullptr);
}

int poller::enable(int sock_fd, void* ud, bool enable_read, bool enable_write)
{
    int ret = 0;
    struct kevent ke;
    EV_SET(&ke, sock_fd, EVFILT_READ, enable_read ? EV_ENABLE : EV_DISABLE, 0, 0, ud);
    if (kevent(poll_fd_, &ke, 1, NULL, 0, NULL) == -1 || ke.flags & EV_ERROR)
    {
        ret |= 1;
    }
    EV_SET(&ke, sock_fd, EVFILT_WRITE, enable_write ? EV_ENABLE : EV_DISABLE, 0, 0, ud);
    if (kevent(poll_fd_, &ke, 1, NULL, 0, NULL) == -1 || ke.flags & EV_ERROR)
    {
        ret |= 1;
    }

    return ret;
}

int poller::wait(event* event_ptr, int max_events/* = MAX_WAIT_EVENT*/)
{
    struct kevent ev[max_events];
    
    int n = ::kevent(poll_fd_, nullptr, 0, ev, max_events, nullptr);
    for (int i = 0; i < n; i++)
    {
        event_ptr[i].socket_ptr = (socket*)ev[i].udata;
        uint32_t filter = ev[i].filter;
        bool is_eof = (ev[i].flags & EV_EOF) != 0;
        event_ptr[i].is_writeable = (filter == EVFILT_WRITE) && (!is_eof);
        event_ptr[i].is_readable = (filter == EVFILT_READ);
        event_ptr[i].is_error = (ev[i].flags & EV_ERROR) != 0;
        event_ptr[i].is_eof = is_eof;
    }

    return n;
}

}

#endif

