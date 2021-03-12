#pragma once

#include "../socket/socket_info.h"
#include "../socket/buffer.h"

namespace skynet {


struct skynet_context;

// skynet socket事件类型
#define SKYNET_SOCKET_TYPE_DATA         1   // 正常数据
#define SKYNET_SOCKET_TYPE_CONNECT      2   // 连接
#define SKYNET_SOCKET_TYPE_CLOSE        3   // 关闭
#define SKYNET_SOCKET_TYPE_ACCEPT       4   //
#define SKYNET_SOCKET_TYPE_ERROR        5   // 错误
#define SKYNET_SOCKET_TYPE_UDP          6   //
#define SKYNET_SOCKET_TYPE_WARNING      7   //

// skynet_socket服务间传递消息结构
struct skynet_socket_message
{
    int type;                   // socket事件类型
    int id;                     //
    int ud;                     // for accept, ud is new connection id ; for data, ud is size of data
    char* buffer;               // 消息携带数据
};

void skynet_socket_init();
void skynet_socket_exit();
void skynet_socket_free();
int skynet_socket_poll();
void skynet_socket_updatetime();

int skynet_socket_sendbuffer(skynet_context* ctx, struct socket::send_buffer *buffer);
int skynet_socket_sendbuffer_lowpriority(skynet_context* ctx, struct socket::send_buffer *buffer);
int skynet_socket_listen(skynet_context* ctx, const char *host, int port, int backlog);
int skynet_socket_connect(skynet_context* ctx, const char *host, int port);
int skynet_socket_bind(skynet_context* ctx, int fd);
void skynet_socket_close(skynet_context* ctx, int id);
void skynet_socket_shutdown(skynet_context* ctx, int id);
void skynet_socket_start(skynet_context* ctx, int id);
void skynet_socket_nodelay(skynet_context* ctx, int id);

int skynet_socket_udp(skynet_context* ctx, const char * addr, int port);
int skynet_socket_udp_connect(skynet_context* ctx, int id, const char * addr, int port);
int skynet_socket_udp_sendbuffer(skynet_context* ctx, const char * address, struct socket::send_buffer *buffer);
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

static inline int skynet_socket_send(skynet_context* ctx, int id, void *buffer, int sz)
{
    socket::send_buffer tmp;
    sendbuffer_init_(&tmp, id, buffer, sz);
    return skynet_socket_sendbuffer(ctx, &tmp);
}

static inline int skynet_socket_send_low_priority(skynet_context* ctx, int id, void *buffer, int sz)
{
    socket::send_buffer tmp;
    sendbuffer_init_(&tmp, id, buffer, sz);
    return skynet_socket_sendbuffer_lowpriority(ctx, &tmp);
}

static inline int skynet_socket_udp_send(skynet_context* ctx, int id, const char * address, const void *buffer, int sz)
{
    socket::send_buffer tmp;
    sendbuffer_init_(&tmp, id, buffer, sz);
    return skynet_socket_udp_sendbuffer(ctx, address, &tmp);
}


}
