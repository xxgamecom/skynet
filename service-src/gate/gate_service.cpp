#include "gate_service.h"
#include "skynet.h"
#include "gate_ctrl_cmd.h"

#include <cassert>
#include <cstdarg>
#include <regex>

namespace skynet::service {

#define DEFAULT_BACKLOG 128

static void _report(gate_service* svc_ptr, const char* data, ...)
{
    if (svc_ptr->watchdog_svc_handle_ == 0)
        return;

    char tmp[1024] = { 0 };
    va_list ap;
    va_start(ap, data);
    int n = ::vsnprintf(tmp, sizeof(tmp), data, ap);
    va_end(ap);

    service_manager::instance()->send(svc_ptr->svc_ctx_, 0, svc_ptr->watchdog_svc_handle_,
                                      SERVICE_MSG_TYPE_TEXT, 0, tmp, n);
}

static void _forward(gate_service* svc_ptr, connection* c, int size)
{
    int socket_id = c->socket_id;

    // socket error
    if (socket_id <= 0)
        return;

    //
    if (svc_ptr->broker_svc_handle_ != 0)
    {
        char* temp = new char[size];
        data_buffer_read(&c->buffer, &svc_ptr->mp_, temp, size);
        service_manager::instance()->send(svc_ptr->svc_ctx_, 0, svc_ptr->broker_svc_handle_,
                                          svc_ptr->svc_msg_type_ | MESSAGE_TAG_DONT_COPY, socket_id, temp, size);
        return;
    }

    if (c->agent_svc_handle != 0)
    {
        char* temp = new char[size];
        data_buffer_read(&c->buffer, &svc_ptr->mp_, temp, size);
        service_manager::instance()->send(svc_ptr->svc_ctx_, c->client_svc_handle, c->agent_svc_handle,
                                          svc_ptr->svc_msg_type_ | MESSAGE_TAG_DONT_COPY, socket_id, temp, size);
    }
    else if (svc_ptr->watchdog_svc_handle_ != 0)
    {
        char* tmp = new char[size + 32];
        int n = snprintf(tmp, 32, "%d data ", c->socket_id);
        data_buffer_read(&c->buffer, &svc_ptr->mp_, tmp + n, size);
        service_manager::instance()->send(svc_ptr->svc_ctx_, 0, svc_ptr->watchdog_svc_handle_,
                                          SERVICE_MSG_TYPE_TEXT | MESSAGE_TAG_DONT_COPY, socket_id, tmp, size + n);
    }
}

static void _do_dispatch_message(gate_service* svc_ptr, connection* c, int id, void* data, int sz)
{
    data_buffer_push(&c->buffer, &svc_ptr->mp_, (char*)data, sz);

    for (;;)
    {
        int size = data_buffer_readheader(&c->buffer, &svc_ptr->mp_, svc_ptr->header_size_);
        if (size < 0)
            return;

        if (size > 0)
        {
            if (size >= 0x1000000)
            {
                data_buffer_clear(&c->buffer, &svc_ptr->mp_);
                node_socket::instance()->close(svc_ptr->svc_ctx_, id);
                log_warn(svc_ptr->svc_ctx_, "Recv socket message > 16M");
                return;
            }
            else
            {
                _forward(svc_ptr, c, size);
                data_buffer_reset(&c->buffer);
            }
        }
    }
}

static void _dispatch_socket_message(gate_service* svc_ptr, const skynet_socket_message* msg_ptr, int sz)
{
    switch (msg_ptr->socket_event)
    {
    case SKYNET_SOCKET_EVENT_DATA:
    {
        int idx = hash_id_lookup(&svc_ptr->hash_, msg_ptr->socket_id);
        if (idx >= 0)
        {
            connection* c = &svc_ptr->connections_.get()[idx];
            _do_dispatch_message(svc_ptr, c, msg_ptr->socket_id, msg_ptr->buffer, msg_ptr->ud);
        }
        else
        {
            log_warn(svc_ptr->svc_ctx_, fmt::format("Drop unknown connection {} message", msg_ptr->socket_id));
            node_socket::instance()->close(svc_ptr->svc_ctx_, msg_ptr->socket_id);
            delete[] msg_ptr->buffer;
        }
        break;
    }
    case SKYNET_SOCKET_EVENT_CONNECT:
    {
        // start listening
        if (msg_ptr->socket_id == svc_ptr->listen_id_)
            break;

        int idx = hash_id_lookup(&svc_ptr->hash_, msg_ptr->socket_id);
        if (idx < 0)
        {
            log_warn(svc_ptr->svc_ctx_, fmt::format("Close unknown connection {}", msg_ptr->socket_id));
            node_socket::instance()->close(svc_ptr->svc_ctx_, msg_ptr->socket_id);
        }
        break;
    }
    case SKYNET_SOCKET_EVENT_CLOSE:
    case SKYNET_SOCKET_EVENT_ERROR:
    {
        int idx = hash_id_remove(&svc_ptr->hash_, msg_ptr->socket_id);
        if (idx >= 0)
        {
            connection* c = &svc_ptr->connections_.get()[idx];
            data_buffer_clear(&c->buffer, &svc_ptr->mp_);
            ::memset(c, 0, sizeof(*c));
            c->socket_id = -1;
            _report(svc_ptr, "%d close", msg_ptr->socket_id);
        }
        break;
    }
    case SKYNET_SOCKET_EVENT_ACCEPT:
    {
        // report accept, then it will be get a SKYNET_SOCKET_EVENT_CONNECT message
        assert(svc_ptr->listen_id_ == msg_ptr->socket_id);

        // reach max connection limit, reject
        if (hash_id_full(&svc_ptr->hash_))
        {
            node_socket::instance()->close(svc_ptr->svc_ctx_, msg_ptr->ud);
            break;
        }

        int idx = hash_id_insert(&svc_ptr->hash_, msg_ptr->ud);
        connection* c = &svc_ptr->connections_.get()[idx];
        if (sz >= sizeof(c->remote_name))
        {
            sz = sizeof(c->remote_name) - 1;
        }
        c->socket_id = msg_ptr->ud;
        ::memcpy(c->remote_name, msg_ptr + 1, sz);
        c->remote_name[sz] = '\0';
        _report(svc_ptr, "%d open %d %s:0", c->socket_id, c->socket_id, c->remote_name);
        log_info(svc_ptr->svc_ctx_, fmt::format("socket open: {:x}", c->socket_id));

        break;
    }
    case SKYNET_SOCKET_EVENT_WARNING:
        log_warn(svc_ptr->svc_ctx_, fmt::format("fd ({}) send buffer ({})K", msg_ptr->socket_id, msg_ptr->ud));
        break;
    }
}

//
static int _start_listen(gate_service* svc_ptr, const std::string& listen_addr)
{
    // split by ':'
    std::regex re {':'};
    std::vector<std::string> host_info {
        std::sregex_token_iterator(listen_addr.begin(), listen_addr.end(), re, -1),
        std::sregex_token_iterator()
    };
    if (host_info.empty())
    {
        log_error(svc_ptr->svc_ctx_, fmt::format("Invalid gate address {}", listen_addr.c_str()));
        return 1;
    }

    std::string host;
    std::string port_string;

    // only provide port
    if (host_info.size() == 1)
    {
        port_string = host_info[0];
    }
    else
    {
        host = host_info[0];
        port_string = host_info[1];
    }

    int port = 0;
    try
    {
        port = std::stoi(port_string);
    }
    catch (...)
    {
        log_error(svc_ptr->svc_ctx_, fmt::format("Invalid gate address {}", listen_addr.c_str()));
        return 1;
    }

    // listen
    svc_ptr->listen_id_ = node_socket::instance()->listen(svc_ptr->svc_ctx_, host.c_str(), port, DEFAULT_BACKLOG);
    if (svc_ptr->listen_id_ < 0)
    {
        return 1;
    }

    // start
    node_socket::instance()->start(svc_ptr->svc_ctx_, svc_ptr->listen_id_);

    return 0;
}

gate_service::~gate_service()
{
    fini();
}

bool gate_service::init(service_context* svc_ctx, const char* param)
{
    bool is_error = true;
    do
    {
        if (param == nullptr)
            break;

        std::string param_string = param;

        // split param by ' '
        std::regex re {' '};
        std::vector<std::string> param_array {
            std::sregex_token_iterator(param_string.begin(), param_string.end(), re, -1),
            std::sregex_token_iterator()
        };
        if (param_array.size() < 4)
        {
            log_error(svc_ctx, fmt::format("Invalid gate param {}", param));
            break;
        }

        // param 1 - package header
        char header = param_array[0].at(0);
        if (header != 'S' && header != 'L')
        {
            log_error(svc_ctx, "Invalid data header style");
            break;
        }

        // param 2 - watchdog
        std::string watchdog = param_array[1];
        if (watchdog[0] == '!')
        {
            watchdog_svc_handle_ = 0;
        }
        else
        {
            watchdog_svc_handle_ = service_manager::instance()->query_by_name(svc_ctx, watchdog.c_str());
            if (watchdog_svc_handle_ == 0)
            {
                log_error(svc_ctx, fmt::format("Invalid watchdog {}", watchdog.c_str()));
                break;
            }
        }

        // param 3 - listen address
        std::string listen_addr = param_array[2];

        // param 4 - message protocol type
        int svc_msg_type = 0;
        try
        {
            svc_msg_type = std::stoi(param_array[3]);
        }
        catch (...)
        {
            log_error(svc_ctx, fmt::format("Invalid gate param {}", param));
            break;
        }
        if (svc_msg_type == 0)
        {
            svc_msg_type = SERVICE_MSG_TYPE_CLIENT;
        }

        // param 5 - max connnection
        int max = 0;
        try
        {
            max = std::stoi(param_array[4]);
        }
        catch (...)
        {
            log_error(svc_ctx, "Invalid gate param, need max connection param");
            break;
        }
        if (max <= 0)
        {
            log_error(svc_ctx, "Invalid gate param, need max connection param");
            break;
        }

        // set mod service context
        this->svc_ctx_ = svc_ctx;

        // alloc connection array
        hash_id_init(&this->hash_, max);
        this->connections_.reset(new connection[max], std::default_delete<connection[]>());
        this->max_connection_ = max;

        //
        this->svc_msg_type_ = svc_msg_type;
        this->header_size_ = header == 'S' ? 2 : 4;

        //
        svc_ctx->set_callback(gate_cb, this);

        is_error = _start_listen(this, listen_addr);
    } while (0);

    return is_error;
}

void gate_service::fini()
{
    // clean connection
    for (int i = 0; i < max_connection_; i++)
    {
        connection* c = &connections_.get()[i];
        if (c->socket_id >= 0)
        {
            node_socket::instance()->close(svc_ctx_, c->socket_id);
        }
    }
    connections_.reset();
    max_connection_ = 0;

    // stop listen
    if (listen_id_ >= 0)
    {
        node_socket::instance()->close(svc_ctx_, listen_id_);
    }
    listen_id_ = -1;

    // clean buffer
    messagepool_free(&mp_);
    hash_id_clear(&hash_);
}

void gate_service::signal(int signal)
{
}

int gate_service::gate_cb(service_context* svc_ctx, void* ud, int svc_msg_type, int session_id, uint32_t src_svc_handle, const void* msg, size_t msg_sz)
{
    auto svc_ptr = (gate_service*)ud;

    switch (svc_msg_type)
    {
    case SERVICE_MSG_TYPE_TEXT:
        handle_ctrl_cmd(svc_ptr, (const char*)msg, (int)msg_sz);
        break;
    case SERVICE_MSG_TYPE_CLIENT:
    {
        if (msg_sz <= 4)
        {
            log_error(svc_ctx, fmt::format("Invalid client message from {:x}", src_svc_handle));
            break;
        }

        // The last 4 bytes in msg are the socket id, write following bytes to it
        const uint8_t* socket_id_ptr = (uint8_t*)msg + msg_sz - 4;
        uint32_t socket_id = socket_id_ptr[0] | socket_id_ptr[1] << 8 | socket_id_ptr[2] << 16 | socket_id_ptr[3] << 24;
        int idx = hash_id_lookup(&svc_ptr->hash_, socket_id);
        if (idx >= 0)
        {
            // don't send id (last 4 bytes)
            skynet_socket_send(svc_ctx, socket_id, (void*)msg, msg_sz - 4);
            // return 1 means don't free msg
            return 1;
        }
        else
        {
            log_error(svc_ctx, fmt::format("Invalid client id {} from {:x}", (int)socket_id, src_svc_handle));
            break;
        }
    }
    case SERVICE_MSG_TYPE_SOCKET:
        // recv socket message from skynet_socket
        _dispatch_socket_message(svc_ptr, (const skynet_socket_message*)msg, (int)(msg_sz - sizeof(skynet_socket_message)));
        break;
    }

    return 0;
}

}

