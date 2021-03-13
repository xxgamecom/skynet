#include "skynet_socket.h"

#include "../socket/server.h"
#include "../timer/timer_manager.h"
#include "../context/service_context.h"

#include <iostream>
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <cstdbool>

namespace skynet {

static std::shared_ptr<socket::server> SOCKET_SERVER;

void skynet_socket_init()
{
    SOCKET_SERVER = std::make_shared<socket::server>();
    if (!SOCKET_SERVER->init(timer_manager::instance()->now()))
    {
        std::cerr << "socket-server : init failed." << std::endl;
        SOCKET_SERVER.reset();
        return;
    }
}

void skynet_socket_exit()
{
    SOCKET_SERVER->exit();
}

void skynet_socket_free()
{
    SOCKET_SERVER.reset();
}

void skynet_socket_updatetime()
{
    SOCKET_SERVER->update_time(timer_manager::instance()->now());
}

// mainloop thread
// 将数据变为 skynet_message 并且将消息压入 二级队列 （每个服务模块的私有队列）
static void forward_message(int type, bool padding, socket::socket_message* result)
{
    size_t sz = sizeof(skynet_socket_message);

    // 
    if (padding)
    {
        if (result->data)
        {
            size_t msg_sz = ::strlen(result->data);
            if (msg_sz > 128)
                msg_sz = 128;
            sz += msg_sz;
        }
        else
        {
            result->data = const_cast<char*>("");
        }
    }

    // skynet_socket_message* sm = (skynet_socket_message*)skynet_malloc(sz);
//     sm->type = type;
//     sm->socket_id = result->socket_id;
//     sm->ud = result->ud;
//     if (padding)
//     {
//         sm->buffer = NULL;
//         ::memcpy(sm+1, result->data, sz - sizeof(*sm));
//     }
//     else
//     {
//         sm->buffer = result->data;
//     }

    // socket::skynet_message message;
    // message.src_svc_handle = 0;
    // message.session = 0;
//     message.data = sm;
//     message.sz = sz | ((size_t)PTYPE_SOCKET << MESSAGE_TYPE_SHIFT);
    
//     if (skynet_context_push((uint32_t)result->svc_handle, &message))
//     {
//         // todo: report somewhere to close socket
//         // don't call skynet_socket_close here (It will block mainloop)
//         skynet_free(sm->buffer);
//         skynet_free(sm);
//     }
}

// 主要工作是将 poll_socket_event 的数据 转换成 skynet 通信机制中使用的格式
// 以便分发数据 socket_sever_poll 返回的数据是  socket_message
// 在 forward_message 中将数据变为 skynet_message 并且将消息压入 二级队列 （每个服务模块的私有队列）
int skynet_socket_poll()
{
    assert(SOCKET_SERVER != nullptr);

    socket::socket_message result;
    bool is_more = true;
    int type = SOCKET_SERVER->poll_socket_event(&result, is_more);
    switch (type)
    {
    case socket::socket_event::SOCKET_EXIT:
        return 0;
    case socket::socket_event::SOCKET_DATA:
        forward_message(skynet_socket_event::SOCKET_DATA, false, &result);
        break;
    case socket::socket_event::SOCKET_CLOSE:
        forward_message(skynet_socket_event::SOCKET_CLOSE, false, &result);
        break;
    case socket::socket_event::SOCKET_OPEN:
        forward_message(skynet_socket_event::SOCKET_CONNECT, true, &result);
        break;
    case socket::socket_event::SOCKET_ERROR:
        forward_message(skynet_socket_event::SOCKET_ERROR, true, &result);
        break;
    case socket::socket_event::SOCKET_ACCEPT:
        forward_message(skynet_socket_event::SOCKET_ACCEPT, true, &result);
        break;
    case socket::socket_event::SOCKET_UDP:
        forward_message(skynet_socket_event::SOCKET_UDP, false, &result);
        break;
    case socket::socket_event::SOCKET_WARNING:
        forward_message(skynet_socket_event::SOCKET_WARNING, false, &result);
        break;
    default:
        //log(nullptr, "Unknown socket message type %d.", type);
        return -1;
    }

    // more event, continue processing
    if (is_more)
        return -1;

    return 1;
}

int skynet_socket_sendbuffer(service_context* ctx, socket::send_buffer* buffer)
{
    return SOCKET_SERVER->send(buffer);
}

int skynet_socket_sendbuffer_lowpriority(service_context* ctx, socket::send_buffer* buffer)
{
    return SOCKET_SERVER->send_low_priority(buffer);
}

int skynet_socket_listen(service_context* ctx, const char* host, int port, int backlog)
{
    uint32_t src_svc_handle = ctx->svc_handle_;
    return SOCKET_SERVER->listen(src_svc_handle, host, port, backlog);
}

int skynet_socket_connect(service_context* ctx, const char *host, int port)
{
    uint32_t src_svc_handle = ctx->svc_handle_;
    return SOCKET_SERVER->connect(src_svc_handle, host, port);
}

int skynet_socket_bind(service_context* ctx, int fd)
{
    uint32_t src_svc_handle = ctx->svc_handle_;
    return SOCKET_SERVER->bind(src_svc_handle, fd);
}

void skynet_socket_close(service_context* ctx, int id)
{
    uint32_t src_svc_handle = ctx->svc_handle_;
    SOCKET_SERVER->close(src_svc_handle, id);
}

void skynet_socket_shutdown(service_context* ctx, int id)
{
    uint32_t src_svc_handle = ctx->svc_handle_;
    SOCKET_SERVER->shutdown(src_svc_handle, id);
}

void skynet_socket_start(service_context* ctx, int id)
{
    uint32_t src_svc_handle = ctx->svc_handle_;
    SOCKET_SERVER->start(src_svc_handle, id);
}

void skynet_socket_nodelay(service_context* ctx, int id)
{
    SOCKET_SERVER->nodelay(id);
}

int skynet_socket_udp(service_context* ctx, const char* addr, int port)
{
    uint32_t src_svc_handle = ctx->svc_handle_;
    return SOCKET_SERVER->socket_server_udp(src_svc_handle, addr, port);
}

int skynet_socket_udp_connect(service_context* ctx, int id, const char* addr, int port)
{
    return SOCKET_SERVER->udp_connect(id, addr, port);
}

// int skynet_socket_udp_sendbuffer(service_context* ctx, const char* address, socket::send_buffer* buffer)
// {
//     return SOCKET_SERVER->udp_send((const struct socket_udp_address*)address, buffer);
// }

const char* skynet_socket_udp_address(skynet_socket_message* msg, int* addrsz)
{
    if (msg->socket_event != skynet_socket_event::SOCKET_UDP)
        return nullptr;

    socket::socket_message sm;
    sm.socket_id = msg->socket_id;
    sm.svc_handle = 0;
    sm.ud = msg->ud;
    sm.data = msg->buffer;
    return (const char*)SOCKET_SERVER->udp_address(&sm, addrsz);
}

socket::socket_info* skynet_socket_info()
{
    return SOCKET_SERVER->get_socket_info();
}

}
