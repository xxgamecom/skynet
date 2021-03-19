#pragma once

#include "../socket/socket_info.h"
#include "../socket/buffer.h"

#include <memory>
#include <list>

namespace skynet {

// skynet node socket event type
enum skynet_socket_event
{
    SKYNET_SOCKET_EVENT_DATA    = 1,                        // data event
    SKYNET_SOCKET_EVENT_CONNECT = 2,                        // connect event
    SKYNET_SOCKET_EVENT_CLOSE   = 3,                        // socket close event
    SKYNET_SOCKET_EVENT_ACCEPT  = 4,                        // accept connection event
    SKYNET_SOCKET_EVENT_ERROR   = 5,                        // socket error event
    SKYNET_SOCKET_EVENT_UDP     = 6,                        //
    SKYNET_SOCKET_EVENT_WARNING = 7,                        //
};


// skynet socket message
struct skynet_socket_message
{
    int                         socket_event;               // skynet socket event type
    int                         socket_id;                  // 

    int                         ud;                         // user data
                                                            // - for accept, ud is new connection id;
                                                            // - for data, ud is size of data.
    char*                       buffer;                     // message data
};


// forward declare
class service_context;
class socket_server;

// skynet node socket
class node_socket final
{
private:
    static node_socket* instance_;
public:
    static node_socket* instance();

private:
    std::shared_ptr<socket_server>  socket_server_;

public:
    bool init();
    void fini();

public:
    //
    void exit();
    //
    void update_time();

    // poll socket event
    int poll_socket_event();

    //
    int listen(service_context* ctx, const char* host, int port, int backlog);
    int connect(service_context* ctx, const char* host, int port);
    int bind(service_context* ctx, int fd);
    void close(service_context* ctx, int socket_id);
    void shutdown(service_context* ctx, int socket_id);
    void start(service_context* ctx, int socket_id);
    void pause(service_context* ctx, int socket_id);
    void nodelay(service_context* ctx, int socket_id);

    int sendbuffer(service_context* ctx, send_buffer* buffer);
    int sendbuffer_low_priority(service_context* ctx, send_buffer* buffer);

    //
    int udp(service_context* ctx, const char* addr, int port);
    int udp_connect(service_context* ctx, int id, const char* addr, int port);
    int udp_sendbuffer(service_context* ctx, const char* address, send_buffer* buffer);
    const char* udp_address(skynet_socket_message*, int* addrsz);

    void get_socket_info(std::list<socket_info>& si_list);
};

//
// legacy APIs
//

static inline void sendbuffer_init_(send_buffer* buf, int socket_id, const void* buffer, int sz)
{
    buf->socket_id = socket_id;
    buf->data_ptr = buffer;
    if (sz < 0)
    {
        buf->type = BUFFER_TYPE_OBJECT;
    }
    else
    {
        buf->type = BUFFER_TYPE_MEMORY;
    }
    buf->data_size = (size_t) sz;
}

static inline int skynet_socket_send(service_context* ctx, int id, void* buffer, int sz)
{
    send_buffer tmp;
    sendbuffer_init_(&tmp, id, buffer, sz);
    return node_socket::instance()->sendbuffer(ctx, &tmp);
}

static inline int skynet_socket_send_low_priority(service_context* ctx, int id, void* buffer, int sz)
{
    send_buffer tmp;
    sendbuffer_init_(&tmp, id, buffer, sz);
    return node_socket::instance()->sendbuffer_low_priority(ctx, &tmp);
}

static inline int skynet_socket_udp_send(service_context* ctx, int id, const char* address, const void* buffer, int sz)
{
    send_buffer tmp;
    sendbuffer_init_(&tmp, id, buffer, sz);
    return node_socket::instance()->udp_sendbuffer(ctx, address, &tmp);
}

}
