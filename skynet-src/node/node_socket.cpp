#include "node_socket.h"

#include "../log/log.h"

#include "../mq/mq_msg.h"

#include "../timer/timer_manager.h"
#include "../socket/socket_server.h"

#include "../service/service_context.h"
#include "../service/service_manager.h"

#include <iostream>
#include <cassert>
#include <mutex>

namespace skynet {

node_socket* node_socket::instance_ = nullptr;

node_socket* node_socket::instance()
{
    static std::once_flag oc;
    std::call_once(oc, [&]() { instance_ = new node_socket; });

    return instance_;
}

bool node_socket::init()
{
    socket_server_ = std::make_shared<socket_server>();
    if (!socket_server_->init(timer_manager::instance()->now()))
    {
        std::cerr << "socket-server : init failed." << std::endl;
        socket_server_.reset();
        return false;
    }

    return true;
}

void node_socket::fini()
{
    socket_server_.reset();
}

void node_socket::exit()
{
    socket_server_->exit();
}

void node_socket::update_time()
{
    socket_server_->update_time(timer_manager::instance()->now());
}

// mainloop thread
// 将数据变为 service_message 并且将消息压入 二级队列 （每个服务模块的私有队列）
static void forward_message(int socket_event, bool padding, socket_message* result)
{
    size_t sz = sizeof(skynet_socket_message);

    // 
    if (padding)
    {
        if (result->data_ptr != nullptr)
        {
            size_t msg_sz = ::strlen(result->data_ptr);
            if (msg_sz > 128)
                msg_sz = 128;
            sz += msg_sz;
        }
        else
        {
            result->data_ptr = const_cast<char*>("");
        }
    }

     auto sm = (skynet_socket_message*)new char[sz];
     sm->socket_event = socket_event;
     sm->socket_id = result->socket_id;
     sm->ud = result->ud;
     if (padding)
     {
         sm->buffer = nullptr;
         ::memcpy(sm+1, result->data_ptr, sz - sizeof(*sm));
     }
     else
     {
         sm->buffer = result->data_ptr;
     }

     service_message message;
     message.src_svc_handle = 0;
     message.session_id = 0;
     message.data_ptr = sm;
     message.data_size = sz | ((size_t)SERVICE_MSG_TYPE_SOCKET << MESSAGE_TYPE_SHIFT);
    
     if (service_manager::instance()->push_service_message((uint32_t)result->svc_handle, &message))
     {
         // todo: report somewhere to close socket
         // don't call skynet_socket_close here (It will block mainloop)
         delete[] sm->buffer;
         delete[] sm;
     }
}

// 主要工作是将 poll_socket_event 的数据 转换成 skynet 通信机制中使用的格式
// 以便分发数据 socket_sever_poll 返回的数据是  socket_message
// 在 forward_message 中将数据变为 service_message 并且将消息压入 二级队列 （每个服务模块的私有队列）
int node_socket::poll_socket_event()
{
    assert(socket_server_ != nullptr);

    socket_message result;
    bool is_more = true;
    int type = socket_server_->poll_socket_event(&result, is_more);
    switch (type)
    {
    case SOCKET_EVENT_EXIT:
        return 0;
    case SOCKET_EVENT_DATA:
        forward_message(SKYNET_SOCKET_EVENT_DATA, false, &result);
        break;
    case SOCKET_EVENT_CLOSE:
        forward_message(SKYNET_SOCKET_EVENT_CLOSE, false, &result);
        break;
    case SOCKET_EVENT_OPEN:
        forward_message(SKYNET_SOCKET_EVENT_CONNECT, true, &result);
        break;
    case SOCKET_EVENT_ERROR:
        forward_message(SKYNET_SOCKET_EVENT_ERROR, true, &result);
        break;
    case SOCKET_EVENT_ACCEPT:
        forward_message(SKYNET_SOCKET_EVENT_ACCEPT, true, &result);
        break;
    case SOCKET_EVENT_UDP:
        forward_message(SKYNET_SOCKET_EVENT_UDP, false, &result);
        break;
    case SOCKET_EVENT_WARNING:
        forward_message(SKYNET_SOCKET_EVENT_WARNING, false, &result);
        break;
    default:
        log_error(nullptr, fmt::format("Unknown socket message type {}.", type));
        return -1;
    }

    // more event, continue processing
    if (is_more)
        return -1;

    return 1;
}

int node_socket::sendbuffer(service_context* ctx, send_buffer* buffer)
{
    return socket_server_->send(buffer);
}

int node_socket::sendbuffer_low_priority(service_context* ctx, send_buffer* buffer)
{
    return socket_server_->send_low_priority(buffer);
}

int node_socket::listen(service_context* ctx, const char* host, int port, int backlog)
{
    return socket_server_->listen(ctx->svc_handle_, host, port, backlog);
}

int node_socket::connect(service_context* ctx, const char *host, int port)
{
    return socket_server_->connect(ctx->svc_handle_, host, port);
}

int node_socket::bind(service_context* ctx, int fd)
{
    return socket_server_->bind(ctx->svc_handle_, fd);
}

void node_socket::close(service_context* ctx, int socket_id)
{
    socket_server_->close(ctx->svc_handle_, socket_id);
}

void node_socket::shutdown(service_context* ctx, int socket_id)
{
    socket_server_->shutdown(ctx->svc_handle_, socket_id);
}

void node_socket::start(service_context* ctx, int socket_id)
{
    socket_server_->start(ctx->svc_handle_, socket_id);
}

void node_socket::pause(service_context* ctx, int socket_id)
{
    socket_server_->pause(ctx->svc_handle_, socket_id);
}

void node_socket::nodelay(service_context* ctx, int socket_id)
{
    socket_server_->nodelay(socket_id);
}

int node_socket::udp(service_context* ctx, const char* addr, int port)
{
    return socket_server_->udp(ctx->svc_handle_, addr, port);
}

int node_socket::udp_connect(service_context* ctx, int socket_id, const char* addr, int port)
{
    return socket_server_->udp_connect(socket_id, addr, port);
}

int node_socket::udp_sendbuffer(service_context* ctx, const char* address, send_buffer* buffer)
{
    return socket_server_->udp_send((const struct socket_udp_address*)address, buffer);
}

const char* node_socket::udp_address(skynet_socket_message* msg, int* addrsz)
{
    if (msg->socket_event != SKYNET_SOCKET_EVENT_UDP)
        return nullptr;

    socket_message sm;
    sm.socket_id = msg->socket_id;
    sm.svc_handle = 0;
    sm.ud = msg->ud;
    sm.data_ptr = msg->buffer;
    return (const char*)socket_server_->udp_address(&sm, addrsz);
}

void node_socket::get_socket_info(std::list<socket_info>& si_list)
{
    socket_server_->get_socket_info(si_list);
}

}
