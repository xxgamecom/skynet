#pragma once

#include "../socket/socket_info.h"
#include "../socket/buffer.h"

namespace skynet {

// skynet socket event type
enum skynet_socket_event
{
    EVENT_DATA                  = 1,                        // data event
    EVENT_CONNECT               = 2,                        // connect event
    EVENT_CLOSE                 = 3,                        // socket close event
    EVENT_ACCEPT                = 4,                        // accept connection event
    EVENT_ERROR                 = 5,                        // socket error event
    EVENT_UDP                   = 6,                        //
    EVENT_WARNING               = 7,                        //
};


// skynet socket message
struct skynet_socket_message
{
    int                         socket_event;               // skynet socket event type
    int                         socket_id;                  // 

    int                         ud;                         // userdata
                                                            // - for accept, ud is new connection id;
                                                            // - for data, ud is size of data.
    char*                       buffer;                     // message data
};


// forward declare
class service_context;

void skynet_socket_init();
void skynet_socket_exit();
void skynet_socket_free();
int skynet_socket_poll();
void skynet_socket_updatetime();

int skynet_socket_sendbuffer(service_context* ctx, struct socket::send_buffer *buffer);
int skynet_socket_sendbuffer_lowpriority(service_context* ctx, struct socket::send_buffer *buffer);
int skynet_socket_listen(service_context* ctx, const char *host, int port, int backlog);
int skynet_socket_connect(service_context* ctx, const char *host, int port);
int skynet_socket_bind(service_context* ctx, int fd);
void skynet_socket_close(service_context* ctx, int id);
void skynet_socket_shutdown(service_context* ctx, int id);
void skynet_socket_start(service_context* ctx, int id);
void skynet_socket_nodelay(service_context* ctx, int id);

int skynet_socket_udp(service_context* ctx, const char * addr, int port);
int skynet_socket_udp_connect(service_context* ctx, int id, const char * addr, int port);
int skynet_socket_udp_sendbuffer(service_context* ctx, const char * address, struct socket::send_buffer *buffer);
const char * skynet_socket_udp_address(struct skynet_socket_message *, int *addrsz);

socket::socket_info* skynet_socket_info();

//
// legacy APIs, TODO: 废弃
//

static inline void sendbuffer_init_(struct socket::send_buffer* buf, int socket_id, const void *buffer, int sz)
{
    buf->socket_id = socket_id;
    buf->buffer = buffer;
    if (sz < 0)
    {
        buf->type = socket::buffer_type::OBJECT;
    }
    else
    {
        buf->type = socket::buffer_type::MEMORY;
    }
    buf->sz = (size_t)sz;
}

static inline int skynet_socket_send(service_context* ctx, int id, void *buffer, int sz)
{
    socket::send_buffer tmp;
    sendbuffer_init_(&tmp, id, buffer, sz);
    return skynet_socket_sendbuffer(ctx, &tmp);
}

static inline int skynet_socket_send_low_priority(service_context* ctx, int id, void *buffer, int sz)
{
    socket::send_buffer tmp;
    sendbuffer_init_(&tmp, id, buffer, sz);
    return skynet_socket_sendbuffer_lowpriority(ctx, &tmp);
}

static inline int skynet_socket_udp_send(service_context* ctx, int id, const char * address, const void *buffer, int sz)
{
    socket::send_buffer tmp;
    sendbuffer_init_(&tmp, id, buffer, sz);
    return skynet_socket_udp_sendbuffer(ctx, address, &tmp);
}


}
