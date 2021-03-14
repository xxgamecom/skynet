#pragma once

#include "socket_server_def.h"

#include <cstdbool>

namespace skynet { namespace socket {

// 每次返回多少事件
#define MAX_EVENT                   64

// forward declare
class socket;

// event poller
class poller final
{
public:
    // poll event
    struct event
    {
        socket*                     socket_ptr = nullptr;                   // 事件关联的socket对象

        bool                        is_read = false;
        bool                        is_write = false;
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
    //
    bool is_invalid();

    //
    bool add(int sock_fd, void* ud);
    void del(int sock_fd);

    /**
     * 在轮序句柄fd中修改sock注册类型
     * 
     * @param sock_fd socket fd
     * @param ud      user data ptr
     * @param enable_write true: enable write
     *                     false: enable read
     */
    void write(int sock_fd, void* ud, bool enable_write);

    /**
     * wait event
     * 
     * @param event_ptr poll events ptr
     * @param max_events max number of events
     * @return number of events
     */
    int wait(event* event_ptr, int max_events = MAX_EVENT);
};

}}
