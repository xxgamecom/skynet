#include "node_socket.h"

#include "../log/log.h"

#include "../mq/mq_msg.h"
#include "../timer/timer_manager.h"
#include "../socket/socket_server.h"
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
    if (!socket_server_->init(timer_manager::instance()->now_ticks()))
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
    socket_server_->update_time(timer_manager::instance()->now_ticks());
}

// mainloop thread
// 将数据变为 service_message 并且将消息压入 服务私有队列
static void forward_message(int socket_event, bool padding, socket_message* msg)
{
    size_t sz = sizeof(skynet_socket_message);

    // 
    if (padding)
    {
        if (msg->data_ptr != nullptr)
        {
            size_t msg_sz = ::strlen(msg->data_ptr);
            if (msg_sz > 128)
                msg_sz = 128;
            sz += msg_sz;
        }
        else
        {
            msg->data_ptr = const_cast<char*>("");
        }
    }

    auto sm = (skynet_socket_message*)new char[sz];
    sm->socket_event = socket_event;
    sm->socket_id = msg->socket_id;
    sm->ud = msg->ud;
    if (padding)
    {
        sm->buffer = nullptr;
        ::memcpy(sm + 1, msg->data_ptr, sz - sizeof(*sm));
    }
    else
    {
        sm->buffer = msg->data_ptr;
    }

    service_message svc_msg;
    svc_msg.src_svc_handle = 0;
    svc_msg.session_id = 0;
    svc_msg.data_ptr = sm;
    svc_msg.data_size = sz | ((size_t)SERVICE_MSG_TYPE_SOCKET << MESSAGE_TYPE_SHIFT);

    if (service_manager::instance()->push_service_message((uint32_t)msg->svc_handle, &svc_msg))
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

    socket_message msg;
    bool is_more = true;
    int type = socket_server_->poll_socket_event(&msg, is_more);
    switch (type)
    {
    case SOCKET_EVENT_EXIT:
        return 0;
    case SOCKET_EVENT_DATA:
        forward_message(SKYNET_SOCKET_EVENT_DATA, false, &msg);
        break;
    case SOCKET_EVENT_CLOSE:
        forward_message(SKYNET_SOCKET_EVENT_CLOSE, false, &msg);
        break;
    case SOCKET_EVENT_OPEN:
        forward_message(SKYNET_SOCKET_EVENT_CONNECT, true, &msg);
        break;
    case SOCKET_EVENT_ERROR:
        forward_message(SKYNET_SOCKET_EVENT_ERROR, true, &msg);
        break;
    case SOCKET_EVENT_ACCEPT:
        forward_message(SKYNET_SOCKET_EVENT_ACCEPT, true, &msg);
        break;
    case SOCKET_EVENT_UDP:
        forward_message(SKYNET_SOCKET_EVENT_UDP, false, &msg);
        break;
    case SOCKET_EVENT_WARNING:
        forward_message(SKYNET_SOCKET_EVENT_WARNING, false, &msg);
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

int node_socket::sendbuffer(uint32_t svc_handle, send_buffer* buffer)
{
    return socket_server_->send(buffer);
}

int node_socket::sendbuffer_low_priority(uint32_t svc_handle, send_buffer* buffer)
{
    return socket_server_->send_low_priority(buffer);
}

int node_socket::listen(uint32_t svc_handle, const char* host, int port, int backlog)
{
    return socket_server_->listen(svc_handle, host, port, backlog);
}

int node_socket::connect(uint32_t svc_handle, const char* host, int port)
{
    return socket_server_->connect(svc_handle, host, port);
}

void node_socket::close(uint32_t svc_handle, int socket_id)
{
    socket_server_->close(svc_handle, socket_id);
}

void node_socket::shutdown(uint32_t svc_handle, int socket_id)
{
    socket_server_->shutdown(svc_handle, socket_id);
}

void node_socket::start(uint32_t svc_handle, int socket_id)
{
    socket_server_->start(svc_handle, socket_id);
}

void node_socket::pause(uint32_t svc_handle, int socket_id)
{
    socket_server_->pause(svc_handle, socket_id);
}

void node_socket::nodelay(uint32_t svc_handle, int socket_id)
{
    socket_server_->nodelay(socket_id);
}

int node_socket::bind_os_fd(uint32_t svc_handle, int os_fd)
{
    return socket_server_->bind_os_fd(svc_handle, os_fd);
}

int node_socket::udp_socket(uint32_t svc_handle, const char* addr, int port)
{
    return socket_server_->udp_socket(svc_handle, addr, port);
}

int node_socket::udp_connect(uint32_t svc_handle, int socket_id, const char* remote_ip, int remote_port)
{
    return socket_server_->udp_connect(socket_id, remote_ip, remote_port);
}

int node_socket::udp_sendbuffer(uint32_t svc_handle, const char* address, send_buffer* buffer)
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
