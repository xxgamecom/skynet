#define LUA_LIB

#include "skynet.h"

extern "C" {
#include <lua.h>
#include <lauxlib.h>
}

#include <string>
#include <cstdlib>
#include <cstdint>
#include <cassert>

#include <sys/socket.h>
#include <arpa/inet.h>

namespace skynet::luaclib {

#define DEFAULT_BACKLOG     32

#define LARGE_PAGE_NODE     12                  // 2 ** 12 == 4096
#define POOL_SIZE_WARNING   32
#define BUFFER_LIMIT        (256 * 1024)

struct buffer_node
{
    char* msg = nullptr;
    int sz = 0;
    buffer_node* next = nullptr;
};

struct socket_buffer
{
    int size = 0;
    int offset = 0;
    buffer_node* head = nullptr;
    buffer_node* tail = nullptr;
};

static void _return_free_node(lua_State* L, int pool, socket_buffer* sb)
{
    buffer_node* free_node = sb->head;
    sb->offset = 0;
    sb->head = free_node->next;
    if (sb->head == nullptr)
    {
        sb->tail = nullptr;
    }
    lua_rawgeti(L, pool, 1);
    free_node->next = (buffer_node*)lua_touserdata(L, -1);

    lua_pop(L, 1);

    skynet_free(free_node->msg);
    free_node->msg = nullptr;

    free_node->sz = 0;
    lua_pushlightuserdata(L, free_node);
    lua_rawseti(L, pool, 1);
}

//
static void _pop_lstring(lua_State* L, socket_buffer* sb, int sz, int skip)
{
    buffer_node* curr_node_ptr = sb->head;
    if (sz < curr_node_ptr->sz - sb->offset)
    {
        lua_pushlstring(L, curr_node_ptr->msg + sb->offset, sz - skip);
        sb->offset += sz;
        return;
    }
    if (sz == curr_node_ptr->sz - sb->offset)
    {
        lua_pushlstring(L, curr_node_ptr->msg + sb->offset, sz - skip);
        _return_free_node(L, 2, sb);
        return;
    }

    luaL_Buffer b;
    luaL_buffinitsize(L, &b, sz);
    for (;;)
    {
        int bytes = curr_node_ptr->sz - sb->offset;
        if (bytes >= sz)
        {
            if (sz > skip)
            {
                luaL_addlstring(&b, curr_node_ptr->msg + sb->offset, sz - skip);
            }
            sb->offset += sz;
            if (bytes == sz)
            {
                _return_free_node(L, 2, sb);
            }
            break;
        }
        int real_sz = sz - skip;
        if (real_sz > 0)
        {
            luaL_addlstring(&b, curr_node_ptr->msg + sb->offset, (real_sz < bytes) ? real_sz : bytes);
        }
        _return_free_node(L, 2, sb);
        sz -= bytes;
        if (sz == 0)
            break;

        curr_node_ptr = sb->head;
        assert(curr_node_ptr);
    }
    luaL_pushresult(&b);
}

static int _free_pool(lua_State* L)
{
    auto pool = (buffer_node*)lua_touserdata(L, 1);
    int sz = lua_rawlen(L, 1) / sizeof(*pool);
    for (int i = 0; i < sz; i++)
    {
        buffer_node* node = &pool[i];
        if (node->msg != nullptr)
        {
            skynet_free(node->msg);
            node->msg = nullptr;
        }
    }

    return 0;
}

// alloc buffer pool
static int _new_pool(lua_State* L, int sz)
{
    auto pool = (buffer_node*)lua_newuserdata(L, sizeof(buffer_node) * sz);
    for (int i = 0; i < sz; i++)
    {
        pool[i].msg = nullptr;
        pool[i].sz = 0;
        pool[i].next = &pool[i + 1];
    }
    pool[sz - 1].next = nullptr;

    //
    if (luaL_newmetatable(L, "buffer_pool") != 0)
    {
        lua_pushcfunction(L, _free_pool);
        lua_setfield(L, -2, "__gc");
    }
    lua_setmetatable(L, -2);

    return 1;
}

/**
 * alloc socket buffer
 *
 * lua examples:
 * new_buffer = socket_core.new_buffer()
 */
static int l_new_socket_buffer(lua_State* L)
{
    auto sb = (socket_buffer*)lua_newuserdata(L, sizeof(socket_buffer));
    sb->size = 0;
    sb->offset = 0;
    sb->head = nullptr;
    sb->tail = nullptr;

    return 1;
}

/**
 * push data to socket buffer
 *
 * arguments:
 * 1 socket buffer          - userdata (socket_buffer)
 * 2 table pool             - table
 * 3 message                - lightuserdata
 * 4 message size           - integer
 *
 * outputs:
 * size
 *
 * comments:
 * The table pool record all buffers chunk, and the first index [1] is a lightuserdata : free_node.
 * We can always use this pointer for buffer_node.
 * The following ([2] ...) userdata in table pool is the buffer chunk (for buffer_node),
 * we never free them until the VM closed. The size of first chunk ([2]) is 16 buffer_node, and the
 * second size is 32 ... The largest size of chunk is LARGE_PAGE_NODE (4096).
 *
 * l_push_socket_buffer will get a free buffer_node from table pool, and then put the msg/size in it.
 * l_pop_socket_buffer return the buffer_node back to table pool (By calling _return_free_node).
 *
 * lua examples:
 * local sz = socket_core.push(s.buffer, s.buffer_pool, data, size)
 */
static int l_push_socket_buffer(lua_State* L)
{
    // socket buffer
    auto sb = (socket_buffer*)lua_touserdata(L, 1);
    if (sb == nullptr)
    {
        return luaL_error(L, "need buffer object at param 1");
    }

    // message
    char* msg = (char*)lua_touserdata(L, 3);
    if (msg == nullptr)
    {
        return luaL_error(L, "need message block at param 3");
    }

    // table pool
    int pool_index = 2;
    luaL_checktype(L, pool_index, LUA_TTABLE);

    // message size
    int sz = luaL_checkinteger(L, 4);
    //
    lua_rawgeti(L, pool_index, 1);

    // sb pool msg size free_node
    auto free_node = (buffer_node*)lua_touserdata(L, -1);
    lua_pop(L, 1);
    if (free_node == nullptr)
    {
        int tsz = lua_rawlen(L, pool_index);
        if (tsz == 0)
            tsz++;
        int size = 8;
        if (tsz <= LARGE_PAGE_NODE - 3)
            size <<= tsz;
        else
            size <<= LARGE_PAGE_NODE - 3;
        _new_pool(L, size);
        free_node = (buffer_node*)lua_touserdata(L, -1);
        lua_rawseti(L, pool_index, tsz + 1);
        if (tsz > POOL_SIZE_WARNING)
        {
            log_warn(nullptr, fmt::format("Too many socket pool ({})", tsz));
        }
    }
    lua_pushlightuserdata(L, free_node->next);
    lua_rawseti(L, pool_index, 1);    // sb poolt msg size
    free_node->msg = msg;
    free_node->sz = sz;
    free_node->next = nullptr;

    if (sb->head == nullptr)
    {
        assert(sb->tail == nullptr);
        sb->head = sb->tail = free_node;
    }
    else
    {
        sb->tail->next = free_node;
        sb->tail = free_node;
    }
    sb->size += sz;

    // return size
    lua_pushinteger(L, sb->size);
    return 1;
}

/**
 * get a free socket buffer
 *
 * arguments:
 * 1 socket buffer          - userdata (socket_buffer)
 * 2 socket buffer pool     - table
 * 3 size                   - integer
 *
 * outputs:
 * 1
 * 2
 *
 * lua examples:
 * ret = socket_core.pop(s.buffer, s.buffer_pool, sz)
 */
static int l_pop_socket_buffer(lua_State* L)
{
    // socket buffer
    auto sb = (socket_buffer*)lua_touserdata(L, 1);
    if (sb == nullptr)
    {
        return luaL_error(L, "Need buffer object at param 1");
    }

    // socket buffer pool
    luaL_checktype(L, 2, LUA_TTABLE);

    // size
    int sz = luaL_checkinteger(L, 3);
    if (sb->size < sz || sz == 0)
    {
        lua_pushnil(L);
    }
    else
    {
        _pop_lstring(L, sb, sz, 0);
        sb->size -= sz;
    }

    // socket buffer size
    lua_pushinteger(L, sb->size);

    return 2;
}

/**
 * clear socket buffer
 *
 * arguments:
 * 1 socket buffer              - userdata (socket_buffer)
 * 2 table pool                 - table
 *
 * lua examples:
 *
 */
static int l_clear_socket_buffer(lua_State* L)
{
    // socket buffer
    auto sb = (socket_buffer*)lua_touserdata(L, 1);
    if (sb == nullptr)
    {
        if (lua_isnil(L, 1))
        {
            return 0;
        }
        return luaL_error(L, "Need buffer object at param 1");
    }

    //
    luaL_checktype(L, 2, LUA_TTABLE);

    //
    while (sb->head != nullptr)
    {
        _return_free_node(L, 2, sb);
    }
    sb->size = 0;

    return 0;
}

/**
 * drop message
 *
 * arguments:
 * 1 message                -
 * 1 message size           - integer
 *
 * lua examples:
 * socket_core.drop(data, size)
 */
static int l_drop(lua_State* L)
{
    // msg, size
    void* msg = lua_touserdata(L, 1);
    luaL_checkinteger(L, 2);

    //
    skynet_free(msg);

    return 0;
}

/**
 * read some bytes
 *
 * arguments:
 * 1 socket buffer          - userdata (socket_buffer)
 * 2 table pool             - table
 *
 * outputs:
 *
 *
 * lua examples:
 * local ret = socket_core.read_all(s.buffer, s.buffer_pool)
 */
static int l_read_all(lua_State* L)
{
    // socket buffer
    auto sb = (socket_buffer*)lua_touserdata(L, 1);
    if (sb == nullptr)
    {
        return luaL_error(L, "Need buffer object at param 1");
    }

    // table pool
    luaL_checktype(L, 2, LUA_TTABLE);

    //
    luaL_Buffer b;
    luaL_buffinit(L, &b);
    while (sb->head != nullptr)
    {
        buffer_node* curr_node_ptr = sb->head;
        luaL_addlstring(&b, curr_node_ptr->msg + sb->offset, curr_node_ptr->sz - sb->offset);
        _return_free_node(L, 2, sb);
    }

    // return buffer
    luaL_pushresult(&b);
    sb->size = 0;

    return 1;
}

// check line end tag
static bool _check_sep(buffer_node* node, int from, const char* sep, int sep_len)
{
    for (;;)
    {
        int sz = node->sz - from;
        if (sz >= sep_len)
        {
            return ::memcmp(node->msg + from, sep, sep_len) == 0;
        }
        if (sz > 0)
        {
            if (::memcmp(node->msg + from, sep, sz))
            {
                return false;
            }
        }
        node = node->next;
        sep += sz;
        sep_len -= sz;
        from = 0;
    }
}

/**
 * read data until reach end tag
 *
 * arguments:
 * 1 socket buffer              - userdata
 * 2 table pool                 - table (optional)
 * 3 separate, end line tag     - string
 *
 * lua examples:
 * socket_core.read_line(s.buffer, s.buffer_pool, sep)
 */
static int l_read_line(lua_State* L)
{
    // socket buffer
    auto sb = (socket_buffer*)lua_touserdata(L, 1);
    if (sb == nullptr)
    {
        return luaL_error(L, "Need buffer object at param 1");
    }

    // table pool, only check
    bool check = !lua_istable(L, 2);

    // separate string
    size_t sep_len = 0;
    const char* sep = luaL_checklstring(L, 3, &sep_len);

    buffer_node* curr_node_ptr = sb->head;
    if (curr_node_ptr == nullptr)
        return 0;

    int from = sb->offset;
    int bytes = curr_node_ptr->sz - from;
    for (int i = 0; i <= sb->size - (int)sep_len; i++)
    {
        if (_check_sep(curr_node_ptr, from, sep, sep_len))
        {
            if (check)
            {
                lua_pushboolean(L, true);
            }
            else
            {
                _pop_lstring(L, sb, i + sep_len, sep_len);
                sb->size -= i + sep_len;
            }
            return 1;
        }

        ++from;
        --bytes;
        if (bytes == 0)
        {
            curr_node_ptr = curr_node_ptr->next;
            from = 0;
            if (curr_node_ptr == nullptr)
                break;
            bytes = curr_node_ptr->sz;
        }
    }

    return 0;
}

/**
 * read message header
 *
 * arguments:
 *
 *
 * lua examples:
 * socket.header = assert(socket_core.header)
 */
static int l_header(lua_State* L)
{
    size_t len;
    const uint8_t* s = (const uint8_t*)luaL_checklstring(L, 1, &len);
    if (len > 4 || len < 1)
    {
        return luaL_error(L, "Invalid read %s", s);
    }

    size_t sz = 0;
    for (auto i = 0; i < len; i++)
    {
        sz <<= 8;
        sz |= s[i];
    }

    // return size
    lua_pushinteger(L, (lua_Integer)sz);
    return 1;
}

/**
 * convert string to memory data
 *
 */
static int l_str2p(lua_State* L)
{
    //
    size_t sz = 0;
    const char* str = luaL_checklstring(L, 1, &sz);

    // data
    void* ptr = skynet_malloc(sz);
    ::memcpy(ptr, str, sz);
    lua_pushlightuserdata(L, ptr);

    // size
    lua_pushinteger(L, (int)sz);

    return 2;
}

/**
 * unpack skynet socket message (skynet_socket_message)
 *
 * arguments:
 * 1 message            - lightuserdata
 * 2 message size       - integer
 *
 * outputs:
 * 1 socket event       - integer
 * 2 socket id          - integer
 * 3 ud                 - integer
 * 4 message            - string | lightuserdata
 * 5 udp address        - string (optional)
 *
 * lua examples:
 * local _, fd = socket_core.unpack(msg, sz)
 */
static int l_unpack(lua_State* L)
{
    // message
    auto msg = (skynet_socket_message*)lua_touserdata(L, 1);
    // message size
    int size = luaL_checkinteger(L, 2);

    // return
    lua_pushinteger(L, msg->socket_event);
    lua_pushinteger(L, msg->socket_id);
    lua_pushinteger(L, msg->ud);
    if (msg->buffer == nullptr)
    {
        lua_pushlstring(L, (char*)(msg + 1), size - sizeof(*msg));
    }
    else
    {
        lua_pushlightuserdata(L, msg->buffer);
    }
    if (msg->socket_event == SKYNET_SOCKET_EVENT_UDP)
    {
        int addr_sz = 0;
        const char* addr_string = node_socket::instance()->udp_address(msg, &addr_sz);
        if (addr_string != nullptr)
        {
            lua_pushlstring(L, addr_string, addr_sz);
            return 5;
        }
    }

    return 4;
}

//
static void _get_socket_info(lua_State* L, socket_info& si)
{
    // create table 't' and push stack
    lua_newtable(L);

    // t['id'] = si.socket_id
    lua_pushinteger(L, si.socket_id);
    lua_setfield(L, -2, "id");

    // t['address'] = si.svc_handle
    lua_pushinteger(L, si.svc_handle);
    lua_setfield(L, -2, "address");

    if (si.type == SOCKET_INFO_TYPE_LISTEN)
    {
        // t['type'] = 'LISTEN'
        lua_pushstring(L, "LISTEN");
        lua_setfield(L, -2, "type");

        // accept count, t['accept'] = si.recv
        lua_pushinteger(L, si.recv);
        lua_setfield(L, -2, "accept");

        // last accept time, t['rtime'] = si.recv_time
        lua_pushinteger(L, si.recv_time);
        lua_setfield(L, -2, "rtime");

        // endpoint info, t['sock'] = si.endpoint
        if (si.endpoint[0] != 0)
        {
            lua_pushstring(L, si.endpoint);
            lua_setfield(L, -2, "sock");
        }
        return;
    }

    if (si.type == SOCKET_INFO_TYPE_TCP)
    {
        lua_pushstring(L, "TCP");
    }
    else if (si.type == SOCKET_INFO_TYPE_UDP)
    {
        lua_pushstring(L, "UDP");
    }
    else if (si.type == SOCKET_INFO_TYPE_BIND)
    {
        lua_pushstring(L, "BIND");
    }
    else if (si.type == SOCKET_INFO_TYPE_CLOSING)
    {
        lua_pushstring(L, "CLOSING");
    }
    else
    {
        lua_pushstring(L, "UNKNOWN");
        lua_setfield(L, -2, "type");
        return;
    }
    lua_setfield(L, -2, "type");

    lua_pushinteger(L, si.recv);
    lua_setfield(L, -2, "read");

    lua_pushinteger(L, si.send);
    lua_setfield(L, -2, "write");

    lua_pushinteger(L, si.wb_size);
    lua_setfield(L, -2, "wbuffer");

    lua_pushinteger(L, si.recv_time);
    lua_setfield(L, -2, "rtime");

    lua_pushinteger(L, si.send_time);
    lua_setfield(L, -2, "wtime");

    lua_pushboolean(L, si.reading);
    lua_setfield(L, -2, "reading");

    lua_pushboolean(L, si.writing);
    lua_setfield(L, -2, "writing");

    //
    if (si.endpoint[0])
    {
        lua_pushstring(L, si.endpoint);
        lua_setfield(L, -2, "peer");
    }
}

/**
 * query socket info in socket_server
 *
 *  table format:
 *  t = {
 *     1 = {
 *         id = socket_id,
 *         address = svc_handle,
 *         type = 'LISTEN',
 *         accept = si.recv,
 *         rtime = si.recv_time,
 *         sock = si.endpoint,
 *     },
 *     2 = {
 *         id = socket_id,
 *         address = svc_handle,
 *         type = 'TCP',
 *         read = si.recv,
 *         write = si.send,
 *         wbuffer = si.wb_size,
 *         rtime = si.recv_time,
 *         stime = si.send_time,
 *         peer = si.endpoint,
 *     },
 *     ...
 * }
 */
static int l_query_socket_info(lua_State* L)
{
    // query socket info
    std::list<socket_info> si_list;
    node_socket::instance()->get_socket_info(si_list);

    lua_newtable(L);

    //
    int n = 0;
    for (auto& si : si_list)
    {
        _get_socket_info(L, si);
        lua_seti(L, -2, ++n); // t[n] = v and pop v
    }

    return 1;
}

// get host & port
static const char* _address_port(lua_State* L, char* tmp, const char* addr, int port_index, int& port)
{
    assert(addr != nullptr);
    std::string tmp_addr = addr;

    // has port in vm stack
    if (!lua_isnoneornil(L, port_index))
    {
        // port (optional)
        port = luaL_optinteger(L, port_index, 0);
        return addr;
    }

    // maybe the addr string include ip & port, parse.
    const char* host = ::strchr(addr, '[');

    // ipv6: [0000:0000:0000:0000:0000:0000:0000:0000]:port
    if (host != nullptr)
    {
        ++host;
        // ip
        const char* sep = ::strchr(addr, ']');
        if (sep == nullptr)
        {
            luaL_error(L, "Invalid address %s.", addr);
        }
        ::memcpy(tmp, host, sep - host);
        tmp[sep - host] = '\0';
        host = tmp;

        // port
        sep = ::strchr(sep + 1, ':');
        if (sep == nullptr)
        {
            luaL_error(L, "Invalid address %s.", addr);
        }
        port = ::strtoul(sep + 1, nullptr, 10);
    }
        // ipv4: 192.168.0.1:port
    else
    {
        // ip
        const char* sep = ::strchr(addr, ':');
        if (sep == nullptr)
        {
            luaL_error(L, "Invalid address %s.", addr);
        }
        ::memcpy(tmp, addr, sep - addr);
        tmp[sep - addr] = '\0';
        host = tmp;

        // port
        port = ::strtoul(sep + 1, nullptr, 10);
    }

    return host;
}

/**
 * connect to destination service
 *
 * arguments:
 * 1 remote host address    - string, can ipv6, ipv4. can include port.
 *                            ipv6: 0000:0000:0000:0000:0000:0000:0000:0000 | [0000:0000:0000:0000:0000:0000:0000:0000]:port
 *                            ipv4: ip | ip:port
 * 2 port                   - integer
 *
 * outputs:
 * socket_id                - integer
 *
 * lua examples:
 * local socket_id = socket_core.connect(addr, port)   --
 * local socket_id = socket_core.connect(addr)         -- addr include ip & port
 */
static int l_connect(lua_State* L)
{
    // addr
    size_t addr_sz = 0;
    const char* addr = luaL_checklstring(L, 1, &addr_sz);
    if (addr == nullptr)
    {
        return luaL_error(L, "Invalid addr");
    }

    char tmp[addr_sz];
    int port = 0;
    const char* host = _address_port(L, tmp, addr, 2, port);
    if (port == 0)
    {
        return luaL_error(L, "Invalid port");
    }

    // service context upvalue
    auto svc_ctx = (service_context*)lua_touserdata(L, lua_upvalueindex(1));

    // connect to destination service
    int socket_id = node_socket::instance()->connect(svc_ctx->svc_handle_, host, port);

    // return socket_id
    lua_pushinteger(L, socket_id);
    return 1;
}

/**
 * close socket connection
 *
 * arguments:
 * 1 socket id              - integer
 *
 * lua examples:
 * socket_core.close(socket_id)
 */
static int l_close(lua_State* L)
{
    auto svc_ctx = (service_context*)lua_touserdata(L, lua_upvalueindex(1));

    int socket_id = luaL_checkinteger(L, 1);
    node_socket::instance()->close(svc_ctx->svc_handle_, socket_id);

    return 0;
}

/**
 * shutdown socket
 *
 * arguments:
 * 1 socket id              - integer
 *
 * lua examples:
 * socket_core.shutdown(socket_id)
 */
static int l_shutdown(lua_State* L)
{
    auto svc_ctx = (service_context*)lua_touserdata(L, lua_upvalueindex(1));

    int socket_id = luaL_checkinteger(L, 1);
    node_socket::instance()->shutdown(svc_ctx->svc_handle_, socket_id);

    return 0;
}

/**
 * listen
 *
 * arguments:
 * 1 address            - string
 * 2 port               - integer
 * 3 backlog            - integer, optional
 *
 * outputs:
 * 1 listen socket id   - integer
 *
 * lua examples:
 * socket_core.listen(address, port)
 */
static int l_listen(lua_State* L)
{
    auto svc_ctx = (service_context*)lua_touserdata(L, lua_upvalueindex(1));

    // ip
    const char* host = luaL_checkstring(L, 1);
    // port
    int port = luaL_checkinteger(L, 2);
    // backlog (optional)
    int backlog = luaL_optinteger(L, 3, DEFAULT_BACKLOG);

    // listen socket id
    int listen_socket_id = node_socket::instance()->listen(svc_ctx->svc_handle_, host, port, backlog);
    if (listen_socket_id < 0)
    {
        return luaL_error(L, "Listen error");
    }

    lua_pushinteger(L, listen_socket_id);

    return 1;
}

static size_t _count_size(lua_State* L, int msg_index)
{
    size_t tlen = 0;
    for (int i = 1; lua_geti(L, msg_index, i) != LUA_TNIL; ++i)
    {
        size_t len;
        luaL_checklstring(L, -1, &len);
        tlen += len;
        lua_pop(L, 1);
    }
    lua_pop(L, 1);

    return tlen;
}

static void _concat_table(lua_State* L, int msg_index, char* buf_ptr, size_t buf_size)
{
    char* ptr = buf_ptr;
    for (int i = 1; lua_geti(L, msg_index, i) != LUA_TNIL; ++i)
    {
        size_t len;
        const char* str = lua_tolstring(L, -1, &len);
        if (str == nullptr || buf_size < len)
            break;

        ::memcpy(ptr, str, len);
        ptr += len;
        buf_size -= len;
        lua_pop(L, 1);
    }
    if (buf_size != 0)
    {
        skynet_free(buf_ptr);
        luaL_error(L, "Invalid strings table");
    }

    lua_pop(L, 1);
}

// get message to buffer
static void _get_message(lua_State* L, int msg_index, send_buffer& sb)
{
    int data_type = lua_type(L, msg_index);
    switch (data_type)
    {
    case LUA_TUSERDATA:
        // lua full useobject must be a raw pointer,
        // it can't be a socket object or a memory object.
        sb.type = BUFFER_TYPE_RAW_POINTER;
        // data
        sb.data_ptr = lua_touserdata(L, msg_index);
        // data size
        if (lua_isinteger(L, msg_index + 1))
            sb.data_size = lua_tointeger(L, msg_index + 1);
        else
            sb.data_size = lua_rawlen(L, msg_index);
        break;
    case LUA_TLIGHTUSERDATA:
    {
        // size
        int data_sz = -1;
        if (lua_isinteger(L, msg_index + 1))
            data_sz = lua_tointeger(L, msg_index + 1);

        // buffer type
        sb.type = data_sz < 0 ? sb.type = BUFFER_TYPE_OBJECT : sb.type = BUFFER_TYPE_MEMORY;
        // data
        sb.data_ptr = lua_touserdata(L, msg_index);
        // data size
        sb.data_size = (size_t)data_sz;
        break;
    }
    case LUA_TTABLE:
    {
        // concat the table as a string
        size_t data_sz = _count_size(L, msg_index);
        char* data_ptr = (char*)skynet_malloc(data_sz);
        _concat_table(L, msg_index, data_ptr, data_sz);
        sb.type = BUFFER_TYPE_MEMORY;
        sb.data_ptr = data_ptr;
        sb.data_size = data_sz;
        break;
    }
    default:
        sb.type = BUFFER_TYPE_RAW_POINTER;
        sb.data_ptr = luaL_checklstring(L, msg_index, &sb.data_size);
        break;
    }
}

/**
 * send message
 *
 * arguments:
 * 1 socket id          - integer
 * 2 message            - userdata | lightuserdata | table
 *
 * lua examples:
 * socket.write = assert(socket_core.send)
 * socket_core.send(fd, content)
 */
static int l_send(lua_State* L)
{
    auto svc_ctx = (service_context*)lua_touserdata(L, lua_upvalueindex(1));

    // socket id
    int socket_id = luaL_checkinteger(L, 1);

    // message
    send_buffer sb;
    _get_message(L, 2, sb);

    sb.socket_id = socket_id;

    // send
    int err = node_socket::instance()->sendbuffer(svc_ctx->svc_handle_, &sb);

    // return result
    lua_pushboolean(L, !err);
    return 1;
}

/**
 *
 */
static int l_send_low(lua_State* L)
{
    auto svc_ctx = (service_context*)lua_touserdata(L, lua_upvalueindex(1));

    // socket id
    int socket_id = luaL_checkinteger(L, 1);

    // message
    send_buffer sb;
    _get_message(L, 2, sb);

    sb.socket_id = socket_id;

    // send
    int err = node_socket::instance()->sendbuffer_low_priority(svc_ctx->svc_handle_, &sb);

    // return result
    lua_pushboolean(L, !err);
    return 1;
}

/**
 * bind std fd
 *
 * arguments:
 * - os fd              - integer, e.g. stdin
 *
 * outputs:
 * socket id
 *
 * see:
 * read_line()
 *
 * lua examples:
 * function socket.stdin()
 *    return socket.bind(0)
 * end
 */
static int l_bind(lua_State* L)
{
    auto svc_ctx = (service_context*)lua_touserdata(L, lua_upvalueindex(1));

    // std fd
    int fd = luaL_checkinteger(L, 1);

    // bind
    int socket_id = node_socket::instance()->bind(svc_ctx->svc_handle_, fd);

    // return socket id
    lua_pushinteger(L, socket_id);
    return 1;
}

/**
 * start socket
 *
 * arguments:
 * 1 socket id              - integer
 *
 * lua examples:
 * function socket.start(id, func)
 *     socket_core.start(id)
 *     return connect(id, func)
 * end
 */
static int l_start(lua_State* L)
{
    auto svc_ctx = (service_context*)lua_touserdata(L, lua_upvalueindex(1));

    int socket_id = luaL_checkinteger(L, 1);
    node_socket::instance()->start(svc_ctx->svc_handle_, socket_id);

    return 0;
}

/**
 * pause socket, for traffic ctrl
 */
static int l_pause(lua_State* L)
{
    auto ctx = (service_context*)lua_touserdata(L, lua_upvalueindex(1));

    // socket id
    int socket_id = luaL_checkinteger(L, 1);
    // pause
    node_socket::instance()->pause(ctx->svc_handle_, socket_id);

    return 0;
}

/**
 *
 */
static int l_nodelay(lua_State* L)
{
    auto svc_ctx = (service_context*)lua_touserdata(L, lua_upvalueindex(1));

    // socket id
    int socket_id = luaL_checkinteger(L, 1);
    // nodelay
    node_socket::instance()->nodelay(svc_ctx->svc_handle_, socket_id);

    return 0;
}

/**
 * create an udp socket (used for udp server & client)
 */
static int l_udp_socket(lua_State* L)
{
    auto svc_ctx = (service_context*)lua_touserdata(L, lua_upvalueindex(1));

    size_t sz = 0;
    const char* addr = lua_tolstring(L, 1, &sz);
    char tmp[sz];
    int local_port = 0;
    const char* local_ip = nullptr;
    if (addr)
    {
        local_ip = _address_port(L, tmp, addr, 2, local_port);
    }

    // create am udp socket
    int socket_id = node_socket::instance()->udp_socket(svc_ctx->svc_handle_, local_ip, local_port);
    if (socket_id < 0)
    {
        return luaL_error(L, "udp socket init failed");
    }
    lua_pushinteger(L, socket_id);

    return 1;
}

static int l_udp_connect(lua_State* L)
{
    auto svc_ctx = (service_context*)lua_touserdata(L, lua_upvalueindex(1));

    //
    int id = luaL_checkinteger(L, 1);

    size_t sz = 0;
    const char* addr = luaL_checklstring(L, 2, &sz);

    char tmp[sz];
    int port = 0;
    const char* host = nullptr;
    if (addr)
    {
        host = _address_port(L, tmp, addr, 3, port);
    }

    if (node_socket::instance()->udp_connect(svc_ctx->svc_handle_, id, host, port))
    {
        return luaL_error(L, "udp connect failed");
    }

    return 0;
}

static int l_udp_send(lua_State* L)
{
    auto svc_ctx = (service_context*)lua_touserdata(L, lua_upvalueindex(1));

    // socket id
    int socket_id = luaL_checkinteger(L, 1);
    // udp address
    const char* address = luaL_checkstring(L, 2);

    // message
    send_buffer sb;
    sb.socket_id = socket_id;
    _get_message(L, 3, sb);

    // send
    int err = node_socket::instance()->udp_sendbuffer(svc_ctx->svc_handle_, address, &sb);

    // return result
    lua_pushboolean(L, !err);
    return 1;
}

static int l_udp_address(lua_State* L)
{
    size_t sz = 0;
    const uint8_t* addr = (const uint8_t*)luaL_checklstring(L, 1, &sz);

    uint16_t port = 0;
    ::memcpy(&port, addr + 1, sizeof(uint16_t));
    port = ntohs(port);

    const void* src = addr + 3;
    char tmp[256];
    int family;
    if (sz == 1 + 2 + 4)
    {
        family = AF_INET;
    }
    else
    {
        if (sz != 1 + 2 + 16)
        {
            return luaL_error(L, "Invalid udp address");
        }
        family = AF_INET6;
    }
    if (::inet_ntop(family, src, tmp, sizeof(tmp)) == nullptr)
    {
        return luaL_error(L, "Invalid udp address");
    }

    lua_pushstring(L, tmp);
    lua_pushinteger(L, port);

    return 2;
}

}

/**
 * skynet luaclib - skynet.socket.core
 */

#if __cplusplus
extern "C" {
#endif

// functions without service_context
static const luaL_Reg socket_funcs_1[] = {
    { "new_buffer",   skynet::luaclib::l_new_socket_buffer },
    { "push_buffer",  skynet::luaclib::l_push_socket_buffer },
    { "pop_buffer",   skynet::luaclib::l_pop_socket_buffer },
    { "clear_buffer", skynet::luaclib::l_clear_socket_buffer },
    { "drop",         skynet::luaclib::l_drop },
    { "read_all",     skynet::luaclib::l_read_all },
    { "read_line",    skynet::luaclib::l_read_line },
    { "str2p",        skynet::luaclib::l_str2p },
    { "header",       skynet::luaclib::l_header },
    { "info",         skynet::luaclib::l_query_socket_info },
    { "unpack",       skynet::luaclib::l_unpack },

    { nullptr,        nullptr },
};

// need service_context upvalue
static const luaL_Reg socket_funcs_2[] = {
    { "listen",      skynet::luaclib::l_listen },
    { "connect",     skynet::luaclib::l_connect },
    { "close",       skynet::luaclib::l_close },
    { "shutdown",    skynet::luaclib::l_shutdown },
    { "send",        skynet::luaclib::l_send },
    { "lsend",       skynet::luaclib::l_send_low },
    { "bind",        skynet::luaclib::l_bind },
    { "start",       skynet::luaclib::l_start },
    { "pause",       skynet::luaclib::l_pause },
    { "nodelay",     skynet::luaclib::l_nodelay },
    { "udp_socket",  skynet::luaclib::l_udp_socket },
    { "udp_connect", skynet::luaclib::l_udp_connect },
    { "udp_send",    skynet::luaclib::l_udp_send },
    { "udp_address", skynet::luaclib::l_udp_address },

    { nullptr,       nullptr },
};

LUAMOD_API int luaopen_skynet_socket_core(lua_State* L)
{
    luaL_checkversion(L);

    // without service context
    luaL_newlib(L, socket_funcs_1);

    // with service_context upvalue
    lua_getfield(L, LUA_REGISTRYINDEX, "service_context");
    auto svc_ctx = (skynet::service_context*)lua_touserdata(L, -1);
    if (svc_ctx == nullptr)
    {
        return luaL_error(L, "[skynet.socket.core] Init skynet service context first");
    }
    luaL_setfuncs(L, socket_funcs_2, 1);

    return 1;
}

#if __cplusplus
}
#endif
