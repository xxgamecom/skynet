#pragma once

#include "socket_server_def.h"

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
    bool add(int sock_fd, void* ud);
    void del(int sock_fd);

    //
    int enable(int sock_fd, void* ud, bool enable_read, bool enable_write);

    /**
     * wait socket event
     * 
     * @param event_ptr poll events ptr
     * @param max_events max number of events
     * @return number of events
     */
    int wait(event* event_ptr, int max_events = MAX_WAIT_EVENT);
};

}
