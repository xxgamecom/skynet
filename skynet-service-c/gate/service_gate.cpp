#include "skynet.h"
#include "gate_mod.h"

 #include <cstdlib>
 #include <string>
 #include <cassert>
 #include <cstdint>

namespace skynet { namespace service {

#define DEFAULT_BACKLOG 128

static void _param(char* msg, int sz, int cmd_sz)
{
    while (cmd_sz < sz)
    {
        if (msg[cmd_sz] != ' ')
            break;
        ++cmd_sz;
    }
    int i;
    for (i = cmd_sz; i < sz; i++)
    {
        msg[i - cmd_sz] = msg[i];
    }
    msg[i - cmd_sz] = '\0';
}

static void _forward_agent(gate_mod* mod_ptr, int fd, uint32_t agent_svc_handle, uint32_t client_svc_handle)
{
    int socket_id = hashid_lookup(&mod_ptr->hash, fd);
    if (socket_id >= 0)
    {
        connection* agent_ptr = &mod_ptr->conn[socket_id];
        agent_ptr->agent_svc_handle = agent_svc_handle;
        agent_ptr->client_svc_handle = client_svc_handle;
    }
}

static void _handle_ctrl_cmd(gate_mod* mod_ptr, const void* msg, int sz)
{
    if (sz == 0)
        return;

    service_context* ctx = mod_ptr->ctx;

    char tmp[sz + 1];
    ::memcpy(tmp, msg, sz);
    tmp[sz] = '\0';

    int i;
    char* cmd = tmp;
    for (i = 0; i < sz; i++)
    {
        if (cmd[i] == ' ')
        {
            break;
        }
    }
    if (::memcmp(cmd, "kick", i) == 0)
    {
        _param(tmp, sz, i);
        int uid = ::strtol(cmd, NULL, 10);
        int socket_id = hashid_lookup(&mod_ptr->hash, uid);
        if (socket_id >= 0)
        {
            node_socket::instance()->close(ctx, uid);
        }
        return;
    }
    if (::memcmp(cmd, "forward", i) == 0)
    {
        _param(tmp, sz, i);

        char* client = tmp;
        char* idstr = ::strsep(&client, " ");
        if (client == nullptr)
            return;

        int id = ::strtol(idstr, NULL, 10);
        char* agent = ::strsep(&client, " ");
        if (client == nullptr)
            return;

        uint32_t agent_svc_handle = ::strtoul(agent + 1, NULL, 16);
        uint32_t client_svc_handle = ::strtoul(client + 1, NULL, 16);
        _forward_agent(mod_ptr, id, agent_svc_handle, client_svc_handle);
        return;
    }
    if (::memcmp(cmd, "broker", i) == 0)
    {
        _param(tmp, sz, i);
        mod_ptr->broker_svc_handle = service_manager::instance()->query_by_name(ctx, cmd);
        return;
    }
    if (::memcmp(cmd, "start", i) == 0)
    {
        _param(tmp, sz, i);
        int uid = ::strtol(cmd, NULL, 10);
        int socket_id = hashid_lookup(&mod_ptr->hash, uid);
        if (socket_id >= 0)
        {
            node_socket::instance()->start(ctx, uid);
        }
        return;
    }
    if (::memcmp(cmd, "close", i) == 0)
    {
        if (mod_ptr->listen_id >= 0)
        {
            node_socket::instance()->close(ctx, mod_ptr->listen_id);
            mod_ptr->listen_id = -1;
        }
        return;
    }

    log(ctx, "[gate] Unknown command : %s", cmd);
}

static void _report(gate_mod* mod_ptr, const char* data, ...)
{
    if (mod_ptr->watchdog_svc_handle == 0)
    {
        return;
    }

    service_context* ctx = mod_ptr->ctx;
    va_list ap;
    va_start(ap, data);
    char tmp[1024];
    int n = vsnprintf(tmp, sizeof(tmp), data, ap);
    va_end(ap);

    service_manager::instance()->send(ctx, 0, mod_ptr->watchdog_svc_handle, message_protocol_type::PTYPE_TEXT, 0, tmp, n);
}

static void _forward(gate_mod* mod_ptr, connection* c, int size)
{
    int socket_id = c->socket_id;

    // socket error
    if (socket_id <= 0)
        return;

    service_context* ctx = mod_ptr->ctx;

    if (mod_ptr->broker_svc_handle != 0)
    {
        char* temp = (char*)skynet_malloc(size);
        data_buffer_read(&c->buffer, &mod_ptr->mp, temp, size);
        service_manager::instance()->send(ctx, 0, mod_ptr->broker_svc_handle, mod_ptr->client_msg_tag | MESSAGE_TAG_DONT_COPY, socket_id, temp, size);
        return;
    }
    if (c->agent_svc_handle != 0)
    {
        char* temp = (char*)skynet_malloc(size);
        data_buffer_read(&c->buffer, &mod_ptr->mp, temp, size);
        service_manager::instance()->send(ctx, c->client_svc_handle, c->agent_svc_handle, mod_ptr->client_msg_tag | MESSAGE_TAG_DONT_COPY, socket_id, temp, size);
    }
    else if (mod_ptr->watchdog_svc_handle != 0)
    {
        char* tmp = (char*)skynet_malloc(size + 32);
        int n = snprintf(tmp, 32, "%d data ", c->socket_id);
        data_buffer_read(&c->buffer, &mod_ptr->mp, tmp + n, size);
        service_manager::instance()->send(ctx, 0, mod_ptr->watchdog_svc_handle, message_protocol_type::PTYPE_TEXT | MESSAGE_TAG_DONT_COPY, socket_id, tmp, size + n);
    }
}

static void do_dispatch_message(gate_mod* mod_ptr, connection* c, int id, void* data, int sz)
{
    data_buffer_push(&c->buffer, &mod_ptr->mp, (char*)data, sz);
    for (;;)
    {
        int size = data_buffer_readheader(&c->buffer, &mod_ptr->mp, mod_ptr->header_size);
        if (size < 0)
        {
            return;
        }
        else if (size > 0)
        {
            if (size >= 0x1000000)
            {
                service_context* ctx = mod_ptr->ctx;
                data_buffer_clear(&c->buffer, &mod_ptr->mp);
                node_socket::instance()->close(ctx, id);
                log(ctx, "Recv socket message > 16M");
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

static void dispatch_socket_message(gate_mod* mod_ptr, const skynet_socket_message* message, int sz)
{
    service_context* ctx = mod_ptr->ctx;

    switch (message->socket_event)
    {
    case SKYNET_SOCKET_EVENT_DATA:
    {
        int socket_id = hashid_lookup(&mod_ptr->hash, message->socket_id);
        if (socket_id >= 0)
        {
            connection* c = &mod_ptr->conn[socket_id];
            do_dispatch_message(mod_ptr, c, message->socket_id, message->buffer, message->ud);
        }
        else
        {
            log(ctx, "Drop unknown connection %d message", message->socket_id);
            node_socket::instance()->close(ctx, message->socket_id);
            skynet_free(message->buffer);
        }
        break;
    }
    case SKYNET_SOCKET_EVENT_CONNECT:
    {
        // start listening
        if (message->socket_id == mod_ptr->listen_id)
            break;

        int socket_id = hashid_lookup(&mod_ptr->hash, message->socket_id);
        if (socket_id < 0)
        {
            log(ctx, "Close unknown connection %d", message->socket_id);
            node_socket::instance()->close(ctx, message->socket_id);
        }
        break;
    }
    case SKYNET_SOCKET_EVENT_CLOSE:
    case SKYNET_SOCKET_EVENT_ERROR:
    {
        int socket_id = hashid_remove(&mod_ptr->hash, message->socket_id);
        if (socket_id >= 0)
        {
            connection* c = &mod_ptr->conn[socket_id];
            data_buffer_clear(&c->buffer, &mod_ptr->mp);
            ::memset(c, 0, sizeof(*c));
            c->socket_id = -1;
            _report(mod_ptr, "%d close", message->socket_id);
        }
        break;
    }
    case SKYNET_SOCKET_EVENT_ACCEPT:
        // report accept, then it will be get a SKYNET_SOCKET_EVENT_CONNECT message
        assert(mod_ptr->listen_id == message->socket_id);
        if (hashid_full(&mod_ptr->hash))
        {
            node_socket::instance()->close(ctx, message->ud);
        }
        else
        {
            connection* c = &mod_ptr->conn[hashid_insert(&mod_ptr->hash, message->ud)];
            if (sz >= sizeof(c->remote_name))
            {
                sz = sizeof(c->remote_name) - 1;
            }
            c->socket_id = message->ud;
            ::memcpy(c->remote_name, message + 1, sz);
            c->remote_name[sz] = '\0';
            _report(mod_ptr, "%d open %d %s:0", c->socket_id, c->socket_id, c->remote_name);
            log(ctx, "socket open: %x", c->socket_id);
        }
        break;
    case SKYNET_SOCKET_EVENT_WARNING:
        log(ctx, "fd (%d) send buffer (%d)K", message->socket_id, message->ud);
        break;
    }
}

// 处理事件的回调函数
static int _gate_cb(service_context* ctx, void* ud, int msg_ptype, int session, uint32_t source, const void* msg, size_t sz)
{
    gate_mod* mod_ptr = (gate_mod*)ud;

    switch (msg_ptype)
    {
    case message_protocol_type::PTYPE_TEXT: // 接收本地指令
        _handle_ctrl_cmd(mod_ptr, msg, (int)sz);
        break;
    case message_protocol_type::PTYPE_CLIENT:
    {
        if (sz <= 4)
        {
            log(ctx, "Invalid client message from %x", source);
            break;
        }

        // The last 4 bytes in msg are the id of socket, write following bytes to it
        const uint8_t* idbuf = (uint8_t*)msg + sz - 4;
        uint32_t uid = idbuf[0] | idbuf[1] << 8 | idbuf[2] << 16 | idbuf[3] << 24;
        int socket_id = hashid_lookup(&mod_ptr->hash, uid);
        if (socket_id >= 0)
        {
            // don't send id (last 4 bytes)
            skynet_socket_send(ctx, uid, (void*)msg, sz - 4);
            // return 1 means don't free msg
            return 1;
        }
        else
        {
            log(ctx, "Invalid client id %d from %x", (int)uid, source);
            break;
        }
    }
    case message_protocol_type::PTYPE_SOCKET:
        // recv socket message from skynet_socket
        dispatch_socket_message(mod_ptr, (const skynet_socket_message*)msg, (int)(sz - sizeof(struct skynet_socket_message)));
        break;
    }

    return 0;
}

//
static int start_listen(gate_mod* mod_ptr, char* listen_addr)
{
    service_context* ctx = mod_ptr->ctx;

    char* portstr = strrchr(listen_addr, ':');
    const char* host = "";
    int port;
    if (portstr == nullptr)
    {
        port = ::strtol(listen_addr, NULL, 10);
        if (port <= 0)
        {
            log(ctx, "Invalid gate address %s", listen_addr);
            return 1;
        }
    }
    else
    {
        port = ::strtol(portstr + 1, NULL, 10);
        if (port <= 0)
        {
            log(ctx, "Invalid gate address %s", listen_addr);
            return 1;
        }
        portstr[0] = '\0';
        host = listen_addr;
    }

    mod_ptr->listen_id = node_socket::instance()->listen(ctx, host, port, DEFAULT_BACKLOG);
    if (mod_ptr->listen_id < 0)
    {
        return 1;
    }

    node_socket::instance()->start(ctx, mod_ptr->listen_id);

    return 0;
}

//-------------------------------------------
// mod interface
//-------------------------------------------

gate_mod* gate_create()
{
    gate_mod* mod_ptr = (gate_mod*)skynet_malloc(sizeof(*mod_ptr));
    ::memset(mod_ptr, 0, sizeof(*mod_ptr));
    mod_ptr->listen_id = -1;

    return mod_ptr;
}

void gate_release(gate_mod* mod_ptr)
{
    service_context* ctx = mod_ptr->ctx;
    for (int i = 0; i < mod_ptr->max_connection; i++)
    {
        connection* c = &mod_ptr->conn[i];
        if (c->socket_id >= 0)
        {
            node_socket::instance()->close(ctx, c->socket_id);
        }
    }
    if (mod_ptr->listen_id >= 0)
    {
        node_socket::instance()->close(ctx, mod_ptr->listen_id);
    }
    messagepool_free(&mod_ptr->mp);
    hashid_clear(&mod_ptr->hash);
    skynet_free(mod_ptr->conn);
    skynet_free(mod_ptr);
}

int gate_init(gate_mod* mod_ptr, service_context* ctx, char* param)
{
    if (param == nullptr)
        return 1;

    int max = 0;
    int sz = ::strlen(param) + 1;
    char watchdog[sz];
    char binding[sz];
    int client_msg_tag = 0;
    char header;
    int n = ::sscanf(param, "%c %s %s %d %d", &header, watchdog, binding, &client_msg_tag, &max);
    if (n < 4)
    {
        log(ctx, "Invalid gate parm %s", param);
        return 1;
    }
    if (max <= 0)
    {
        log(ctx, "Need max connection");
        return 1;
    }
    if (header != 'S' && header != 'L')
    {
        log(ctx, "Invalid data header style");
        return 1;
    }

    if (client_msg_tag == 0)
    {
        client_msg_tag = message_protocol_type::PTYPE_CLIENT;
    }
    if (watchdog[0] == '!')
    {
        mod_ptr->watchdog_svc_handle = 0;
    }
    else
    {
        mod_ptr->watchdog_svc_handle = service_manager::instance()->query_by_name(ctx, watchdog);
        if (mod_ptr->watchdog_svc_handle == 0)
        {
            log(ctx, "Invalid watchdog %s", watchdog);
            return 1;
        }
    }

    mod_ptr->ctx = ctx;

    hashid_init(&mod_ptr->hash, max);
    mod_ptr->conn = (connection*)skynet_malloc(max * sizeof(connection));
    ::memset(mod_ptr->conn, 0, max * sizeof(connection));
    mod_ptr->max_connection = max;
    for (int i = 0; i < max; i++)
    {
        mod_ptr->conn[i].socket_id = -1;
    }

    mod_ptr->client_msg_tag = client_msg_tag;
    mod_ptr->header_size = header == 'S' ? 2 : 4;

    ctx->set_callback(mod_ptr, _gate_cb);

    return start_listen(mod_ptr, binding);
}

} }
