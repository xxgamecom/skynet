#include "skynet.h"
#include "gate_mod.h"
#include "gate_ctrl_cmd.h"

#include <cstdlib>
#include <string>
#include <cassert>
#include <cstdint>
#include <regex>

namespace skynet { namespace service {

#define DEFAULT_BACKLOG 128

static void _report(gate_mod* mod_ptr, const char* data, ...)
{
    if (mod_ptr->watchdog_svc_handle == 0)
        return;

    char tmp[1024] = { 0 };
    va_list ap;
    va_start(ap, data);
    int n = ::vsnprintf(tmp, sizeof(tmp), data, ap);
    va_end(ap);

    service_manager::instance()->send(mod_ptr->svc_ctx, 0, mod_ptr->watchdog_svc_handle,
        message_protocol_type::PTYPE_TEXT, 0, tmp, n);
}

static void _forward(gate_mod* mod_ptr, connection* c, int size)
{
    int socket_id = c->socket_id;

    // socket error
    if (socket_id <= 0)
        return;

    //
    if (mod_ptr->broker_svc_handle != 0)
    {
        char* temp = (char*)skynet_malloc(size);
        data_buffer_read(&c->buffer, &mod_ptr->mp, temp, size);
        service_manager::instance()->send(mod_ptr->svc_ctx, 0,mod_ptr->broker_svc_handle,
            mod_ptr->msg_ptype | MESSAGE_TAG_DONT_COPY, socket_id, temp, size);
        return;
    }

    if (c->agent_svc_handle != 0)
    {
        char* temp = (char*)skynet_malloc(size);
        data_buffer_read(&c->buffer, &mod_ptr->mp, temp, size);
        service_manager::instance()->send(mod_ptr->svc_ctx, c->client_svc_handle, c->agent_svc_handle,
            mod_ptr->msg_ptype | MESSAGE_TAG_DONT_COPY, socket_id, temp, size);
    }
    else if (mod_ptr->watchdog_svc_handle != 0)
    {
        char* tmp = (char*)skynet_malloc(size + 32);
        int n = snprintf(tmp, 32, "%d data ", c->socket_id);
        data_buffer_read(&c->buffer, &mod_ptr->mp, tmp + n, size);
        service_manager::instance()->send(mod_ptr->svc_ctx, 0, mod_ptr->watchdog_svc_handle,
            message_protocol_type::PTYPE_TEXT | MESSAGE_TAG_DONT_COPY, socket_id, tmp, size + n);
    }
}

static void _do_dispatch_message(gate_mod* mod_ptr, connection* c, int id, void* data, int sz)
{
    data_buffer_push(&c->buffer, &mod_ptr->mp, (char*)data, sz);

    for (;;)
    {
        int size = data_buffer_readheader(&c->buffer, &mod_ptr->mp, mod_ptr->header_size);
        if (size < 0)
            return;

        if (size > 0)
        {
            if (size >= 0x1000000)
            {
                data_buffer_clear(&c->buffer, &mod_ptr->mp);
                node_socket::instance()->close(mod_ptr->svc_ctx, id);
                log(mod_ptr->svc_ctx, "Recv socket message > 16M");
                return;
            }
            else
            {
                _forward(mod_ptr, c, size);
                data_buffer_reset(&c->buffer);
            }
        }
    }
}

static void _dispatch_socket_message(gate_mod* mod_ptr, const skynet_socket_message* msg_ptr, int sz)
{
    switch (msg_ptr->socket_event)
    {
    case SKYNET_SOCKET_EVENT_DATA:
    {
        int idx = hash_id_lookup(&mod_ptr->hash, msg_ptr->socket_id);
        if (idx >= 0)
        {
            connection* c = &mod_ptr->connections.get()[idx];
            _do_dispatch_message(mod_ptr, c, msg_ptr->socket_id, msg_ptr->buffer, msg_ptr->ud);
        }
        else
        {
            log(mod_ptr->svc_ctx, "Drop unknown connection %d message", msg_ptr->socket_id);
            node_socket::instance()->close(mod_ptr->svc_ctx, msg_ptr->socket_id);
            skynet_free(msg_ptr->buffer);
        }
        break;
    }
    case SKYNET_SOCKET_EVENT_CONNECT:
    {
        // start listening
        if (msg_ptr->socket_id == mod_ptr->listen_id)
            break;

        int idx = hash_id_lookup(&mod_ptr->hash, msg_ptr->socket_id);
        if (idx < 0)
        {
            log(mod_ptr->svc_ctx, "Close unknown connection %d", msg_ptr->socket_id);
            node_socket::instance()->close(mod_ptr->svc_ctx, msg_ptr->socket_id);
        }
        break;
    }
    case SKYNET_SOCKET_EVENT_CLOSE:
    case SKYNET_SOCKET_EVENT_ERROR:
    {
        int idx = hash_id_remove(&mod_ptr->hash, msg_ptr->socket_id);
        if (idx >= 0)
        {
            connection* c = &mod_ptr->connections.get()[idx];
            data_buffer_clear(&c->buffer, &mod_ptr->mp);
            ::memset(c, 0, sizeof(*c));
            c->socket_id = -1;
            _report(mod_ptr, "%d close", msg_ptr->socket_id);
        }
        break;
    }
    case SKYNET_SOCKET_EVENT_ACCEPT:
    {
        // report accept, then it will be get a SKYNET_SOCKET_EVENT_CONNECT message
        assert(mod_ptr->listen_id == msg_ptr->socket_id);

        // reach max connection limit, reject
        if (hash_id_full(&mod_ptr->hash))
        {
            node_socket::instance()->close(mod_ptr->svc_ctx, msg_ptr->ud);
            break;
        }

        int idx = hash_id_insert(&mod_ptr->hash, msg_ptr->ud);
        connection* c = &mod_ptr->connections.get()[idx];
        if (sz >= sizeof(c->remote_name))
        {
            sz = sizeof(c->remote_name) - 1;
        }
        c->socket_id = msg_ptr->ud;
        ::memcpy(c->remote_name, msg_ptr + 1, sz);
        c->remote_name[sz] = '\0';
        _report(mod_ptr, "%d open %d %s:0", c->socket_id, c->socket_id, c->remote_name);
        log(mod_ptr->svc_ctx, "socket open: %x", c->socket_id);
        break;
    }
    case SKYNET_SOCKET_EVENT_WARNING:
        log(mod_ptr->svc_ctx, "fd (%d) send buffer (%d)K", msg_ptr->socket_id, msg_ptr->ud);
        break;
    }
}

//
static int _start_listen(gate_mod* mod_ptr, const std::string& listen_addr)
{
    // split by ':'
    std::regex re {':'};
    std::vector<std::string> host_info {
        std::sregex_token_iterator(listen_addr.begin(), listen_addr.end(), re, -1),
        std::sregex_token_iterator()
    };
    if (host_info.empty())
    {
        log(mod_ptr->svc_ctx, "Invalid gate address %s", listen_addr.c_str());
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
        log(mod_ptr->svc_ctx, "Invalid gate address %s", listen_addr.c_str());
        return 1;
    }

    // listen
    mod_ptr->listen_id = node_socket::instance()->listen(mod_ptr->svc_ctx, host.c_str(), port, DEFAULT_BACKLOG);
    if (mod_ptr->listen_id < 0)
    {
        return 1;
    }

    // start
    node_socket::instance()->start(mod_ptr->svc_ctx, mod_ptr->listen_id);

    return 0;
}

/**
 * gate servcie message callback
 */
static int _gate_cb(service_context* svc_ctx, void* ud, int msg_ptype, int session_id, uint32_t src_service_handle, const void* msg, size_t msg_sz)
{
    auto mod_ptr = (gate_mod*)ud;

    switch (msg_ptype)
    {
    case message_protocol_type::PTYPE_TEXT:
        handle_ctrl_cmd(mod_ptr, (const char*)msg, (int)msg_sz);
        break;
    case message_protocol_type::PTYPE_CLIENT:
    {
        if (msg_sz <= 4)
        {
            log(svc_ctx, "Invalid client message from %x", src_service_handle);
            break;
        }

        // The last 4 bytes in msg are the socket id, write following bytes to it
        const uint8_t* socket_id_ptr = (uint8_t*)msg + msg_sz - 4;
        uint32_t socket_id = socket_id_ptr[0] | socket_id_ptr[1] << 8 | socket_id_ptr[2] << 16 | socket_id_ptr[3] << 24;
        int idx = hash_id_lookup(&mod_ptr->hash, socket_id);
        if (idx >= 0)
        {
            // don't send id (last 4 bytes)
            skynet_socket_send(svc_ctx, socket_id, (void*)msg, msg_sz - 4);
            // return 1 means don't free msg
            return 1;
        }
        else
        {
            log(svc_ctx, "Invalid client id %d from %x", (int)socket_id, src_service_handle);
            break;
        }
    }
    case message_protocol_type::PTYPE_SOCKET:
        // recv socket message from skynet_socket
        _dispatch_socket_message(mod_ptr, (const skynet_socket_message*)msg, (int)(msg_sz - sizeof(skynet_socket_message)));
        break;
    }

    return 0;
}

//-------------------------------------------
// c service mod interface
//-------------------------------------------

gate_mod* gate_create()
{
    auto mod_ptr = new gate_mod;
    mod_ptr->listen_id = -1;

    return mod_ptr;
}

void gate_release(gate_mod* mod_ptr)
{
    for (int i = 0; i < mod_ptr->max_connection; i++)
    {
        connection* c = &mod_ptr->connections.get()[i];
        if (c->socket_id >= 0)
        {
            node_socket::instance()->close(mod_ptr->svc_ctx, c->socket_id);
        }
    }
    if (mod_ptr->listen_id >= 0)
    {
        node_socket::instance()->close(mod_ptr->svc_ctx, mod_ptr->listen_id);
    }
    messagepool_free(&mod_ptr->mp);
    hash_id_clear(&mod_ptr->hash);

    delete mod_ptr;
}

int gate_init(gate_mod* mod_ptr, service_context* svc_ctx, char* param)
{
    int is_error = 1;
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
            log(svc_ctx, "Invalid gate parm %s", param);
            break;
        }

        // param 1 - package header
        char header = param_array[0].at(0);
        if (header != 'S' && header != 'L')
        {
            log(svc_ctx, "Invalid data header style");
            break;
        }

        // param 2 - watchdog
        std::string watchdog = param_array[1];
        if (watchdog[0] == '!')
        {
            mod_ptr->watchdog_svc_handle = 0;
        }
        else
        {
            mod_ptr->watchdog_svc_handle = service_manager::instance()->query_by_name(svc_ctx, watchdog.c_str());
            if (mod_ptr->watchdog_svc_handle == 0)
            {
                log(svc_ctx, "Invalid watchdog %s", watchdog.c_str());
                break;
            }
        }

        // param 3 - listen address
        std::string listen_addr = param_array[2];

        // param 4 - message protocol type
        int msg_ptype = 0;
        try
        {
            msg_ptype = std::stoi(param_array[3]);
        }
        catch (...)
        {
            log(svc_ctx, "Invalid gate parm %s", param);
            break;
        }
        if (msg_ptype == 0)
        {
            msg_ptype = message_protocol_type::PTYPE_CLIENT;
        }

        // param 5 - max connnection
        int max = 0;
        try
        {
            max = std::stoi(param_array[4]);
        }
        catch (...)
        {
            log(svc_ctx, "Need max connection");
            break;
        }
        if (max <= 0)
        {
            log(svc_ctx, "Need max connection");
            break;
        }

        // set mod service context
        mod_ptr->svc_ctx = svc_ctx;

        // alloc connection array
        hash_id_init(&mod_ptr->hash, max);
        mod_ptr->connections.reset(new connection[max], std::default_delete<connection[]>());
        mod_ptr->max_connection = max;

        //
        mod_ptr->msg_ptype = msg_ptype;
        mod_ptr->header_size = header == 'S' ? 2 : 4;

        //
        svc_ctx->set_callback(mod_ptr, _gate_cb);

        is_error = _start_listen(mod_ptr, listen_addr);
    } while (0);

    return is_error;
}

} }
