#pragma once

#include "../socket_server_def.h"

#include <cstdbool>

namespace skynet {

// forward declare
class socket;

// event poller
class poller final
{
public:
    // constants
    enum
    {
        MAX_WAIT_EVENT              = 64,                                   // max number of events
    };

    // poll event
    struct event
    {
        socket*                     socket_ptr = nullptr;                   // the socket object associated with the event

        bool                        is_readable = false;
        bool                        is_writeable = false;
        bool                        is_error = false;
        bool                        is_eof = false;
    };

private:
    int                             poll_fd_ = INVALID_FD;

public:
    poller() = default;
    ~poller();

public:
    // initialize
    bool init();
    // clean
    void fini();

public:
    // is poller valid (poll_fd_ != INVALID_FD)
    bool is_valid();

    // add/del socket event detect
    bool add(int socket_fd, void* ud);
    void del(int socket_fd);

    //
    int enable(int socket_fd, void* ud, bool enable_read, bool enable_write);

    /**
     * wait socket event
     * 
     * @param event_ptr poll events ptr
     * @param max_events max number of events
     * @return number of events
     */
    int wait(event* event_ptr, int max_events = MAX_WAIT_EVENT);
};


//
//typedef int poll_fd;
//
//struct event {
//    void * s;
//    bool read;
//    bool write;
//    bool error;
//    bool eof;
//};
//
//static bool sp_invalid(poll_fd fd);
//static poll_fd sp_create();
//static void sp_release(poll_fd fd);
//static int sp_add(poll_fd fd, int sock, void *ud);
//static void sp_del(poll_fd fd, int sock);
//static void sp_write(poll_fd, int sock, void *ud, bool enable);
//static int sp_wait(poll_fd, struct event *e, int max);
//static void sp_nonblocking(int sock);
//
//#ifdef __linux__
//#include "socket_epoll.h"
//#endif
//
//#if defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__) || defined (__NetBSD__)
//#include "socket_kqueue.h"
//#endif

}
