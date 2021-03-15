/**
 * skynet luaclib - skynet.socketdriver
 * 
 */

#define LUA_LIB

#include "skynet.h"

extern "C" {
#include <lua.h>
#include <lauxlib.h>
}

#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <assert.h>

#include <sys/socket.h>
#include <arpa/inet.h>

namespace skynet { namespace luaclib {

#define BACKLOG             32
#define LARGE_PAGE_NODE     12                  // 2 ** 12 == 4096
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

static int l_freepool(lua_State* L)
{
    buffer_node* pool = (buffer_node*)lua_touserdata(L, 1);
    int sz = lua_rawlen(L, 1) / sizeof(*pool);
    for (int i = 0; i < sz; i++)
    {
        buffer_node* node = &pool[i];
        if (node->msg != nullptr)
        {
            delete[] node->msg;
            node->msg = nullptr;
        }
    }
    
    return 0;
}

static int l_newpool(lua_State* L, int sz)
{
    buffer_node* pool = (buffer_node*)lua_newuserdata(L, sizeof(buffer_node) * sz);
    for (int i = 0; i < sz; i++)
    {
        pool[i].msg = nullptr;
        pool[i].sz = 0;
        pool[i].next = &pool[i + 1];
    }
    pool[sz-1].next = nullptr;
    if (luaL_newmetatable(L, "buffer_pool"))
    {
        lua_pushcfunction(L, l_freepool);
        lua_setfield(L, -2, "__gc");
    }
    lua_setmetatable(L, -2);
    
    return 1;
}

static int l_newbuffer(lua_State* L)
{
    socket_buffer* sb = (socket_buffer*)lua_newuserdata(L, sizeof(*sb));	
    sb->size = 0;
    sb->offset = 0;
    sb->head = nullptr;
    sb->tail = nullptr;
    
    return 1;
}

/*
    userdata send_buffer
    table pool
    lightuserdata msg
    int size

    return size

    Comment: The table pool record all the buffers chunk, 
    and the first index [1] is a lightuserdata : free_node. We can always use this pointer for struct buffer_node .
    The following ([2] ...)  userdatas in table pool is the buffer chunk (for struct buffer_node), 
    we never free them until the VM closed. The size of first chunk ([2]) is 16 struct buffer_node,
    and the second size is 32 ... The largest size of chunk is LARGE_PAGE_NODE (4096)

    l_pushbuffer will get a free struct buffer_node from table pool, and then put the msg/size in it.
    l_popbuffer return the struct buffer_node back to table pool (By calling return_free_node).
 */
static int l_pushbuffer(lua_State* L)
{
    socket_buffer* sb = (socket_buffer*)lua_touserdata(L,1);
    if (sb == nullptr)
        return luaL_error(L, "need buffer object at param 1");

    char* msg = (char*)lua_touserdata(L,3);
    if (msg == nullptr)
        return luaL_error(L, "need message block at param 3");
    
    int pool_index = 2;
    luaL_checktype(L,pool_index,LUA_TTABLE);
    int sz = luaL_checkinteger(L,4);
    lua_rawgeti(L,pool_index,1);
    buffer_node* free_node = (buffer_node*)lua_touserdata(L,-1);	// sb poolt msg size free_node
    lua_pop(L, 1);
    if (free_node == nullptr)
    {
        int tsz = lua_rawlen(L,pool_index);
        if (tsz == 0)
            tsz++;
        int size = 8;
        if (tsz <= LARGE_PAGE_NODE-3)
        {
            size <<= tsz;
        }
        else
        {
            size <<= LARGE_PAGE_NODE-3;
        }
        l_newpool(L, size);	
        free_node = (buffer_node*)lua_touserdata(L,-1);
        lua_rawseti(L, pool_index, tsz+1);
    }
    lua_pushlightuserdata(L, free_node->next);	
    lua_rawseti(L, pool_index, 1);	// sb poolt msg size
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

    lua_pushinteger(L, sb->size);

    return 1;
}

static void return_free_node(lua_State* L, int pool, struct socket_buffer *sb)
{
    buffer_node* free_node = sb->head;
    sb->offset = 0;
    sb->head = free_node->next;
    if (sb->head == nullptr)
    {
        sb->tail = nullptr;
    }
    lua_rawgeti(L,pool,1);
    free_node->next = (buffer_node*)lua_touserdata(L,-1);
    
    lua_pop(L, 1);

    delete[] free_node->msg;
    free_node->msg = nullptr;

    free_node->sz = 0;
    lua_pushlightuserdata(L, free_node);
    lua_rawseti(L, pool, 1);
}

static void pop_lstring(lua_State* L, struct socket_buffer *sb, int sz, int skip)
{
    buffer_node* current = sb->head;
    if (sz < current->sz - sb->offset)
    {
        lua_pushlstring(L, current->msg + sb->offset, sz-skip);
        sb->offset+=sz;
        return;
    }
    if (sz == current->sz - sb->offset)
    {
        lua_pushlstring(L, current->msg + sb->offset, sz-skip);
        return_free_node(L,2,sb);
        return;
    }

    luaL_Buffer b;
    luaL_buffinit(L, &b);
    for (;;)
    {
        int bytes = current->sz - sb->offset;
        if (bytes >= sz)
        {
            if (sz > skip)
            {
                luaL_addlstring(&b, current->msg + sb->offset, sz - skip);
            } 
            sb->offset += sz;
            if (bytes == sz)
            {
                return_free_node(L,2,sb);
            }
            break;
        }
        int real_sz = sz - skip;
        if (real_sz > 0)
        {
            luaL_addlstring(&b, current->msg + sb->offset, (real_sz < bytes) ? real_sz : bytes);
        }
        return_free_node(L, 2, sb);
        sz -= bytes;
        if (sz == 0)
            break;
        
        current = sb->head;
        assert(current);
    }
    luaL_pushresult(&b);
}

static int l_header(lua_State* L)
{
    size_t len;
    const uint8_t * s = (const uint8_t *)luaL_checklstring(L, 1, &len);
    if (len > 4 || len < 1)
    {
        return luaL_error(L, "Invalid read %s", s);
    }

    size_t sz = 0;
    for (int i = 0; i < (int)len; i++)
    {
        sz <<= 8;
        sz |= s[i];
    }

    lua_pushinteger(L, (lua_Integer)sz);

    return 1;
}

/*
    userdata send_buffer
    table pool
    integer sz 
 */
static int l_popbuffer(lua_State* L)
{
    socket_buffer* sb = (socket_buffer*)lua_touserdata(L, 1);
    if (sb == nullptr)
    {
        return luaL_error(L, "Need buffer object at param 1");
    }
    luaL_checktype(L, 2, LUA_TTABLE);
    int sz = luaL_checkinteger(L, 3);
    if (sb->size < sz || sz == 0)
    {
        lua_pushnil(L);
    }
    else
    {
        pop_lstring(L,sb,sz,0);
        sb->size -= sz;
    }
    lua_pushinteger(L, sb->size);

    return 2;
}

/*
    userdata send_buffer
    table pool
 */
static int lclearbuffer(lua_State* L)
{
    socket_buffer* sb = (socket_buffer*)lua_touserdata(L, 1);
    if (sb == nullptr)
    {
        if (lua_isnil(L, 1))
        {
            return 0;
        }
        return luaL_error(L, "Need buffer object at param 1");
    }
    luaL_checktype(L, 2, LUA_TTABLE);
    while (sb->head != nullptr)
    {
        return_free_node(L, 2, sb);
    }
    sb->size = 0;
    return 0;
}

static int l_readall(lua_State* L)
{
    socket_buffer* sb = (socket_buffer*)lua_touserdata(L, 1);
    if (sb == nullptr)
    {
        return luaL_error(L, "Need buffer object at param 1");
    }
    luaL_checktype(L, 2, LUA_TTABLE);
    luaL_Buffer b;
    luaL_buffinit(L, &b);
    while(sb->head)
    {
        buffer_node* current = sb->head;
        luaL_addlstring(&b, current->msg + sb->offset, current->sz - sb->offset);
        return_free_node(L, 2, sb);
    }
    luaL_pushresult(&b);
    sb->size = 0;
    return 1;
}

static int l_drop(lua_State* L)
{
    void* msg = lua_touserdata(L,1);
    luaL_checkinteger(L, 2);
    delete msg;
    return 0;
}

static bool check_sep(buffer_node* node, int from, const char *sep, int seplen)
{
    for (;;)
    {
        int sz = node->sz - from;
        if (sz >= seplen)
        {
            return ::memcmp(node->msg+from,sep,seplen) == 0;
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
        seplen -= sz;
        from = 0;
    }
}

/*
    userdata send_buffer
    table pool , nil for check
    string sep
 */
static int l_readline(lua_State* L)
{
    socket_buffer* sb = (socket_buffer*)lua_touserdata(L, 1);
    if (sb == nullptr)
    {
        return luaL_error(L, "Need buffer object at param 1");
    }
    
    // only check
    bool check = !lua_istable(L, 2);
    size_t seplen = 0;
    const char *sep = luaL_checklstring(L,3,&seplen);

    buffer_node* current = sb->head;
    if (current == nullptr)
        return 0;
    int from = sb->offset;
    int bytes = current->sz - from;
    for (int i = 0; i <= sb->size - (int)seplen; i++)
    {
        if (check_sep(current, from, sep, seplen))
        {
            if (check)
            {
                lua_pushboolean(L,true);
            }
            else
            {
                pop_lstring(L, sb, i+seplen, seplen);
                sb->size -= i+seplen;
            }
            return 1;
        }
        ++from;
        --bytes;
        if (bytes == 0)
        {
            current = current->next;
            from = 0;
            if (current == nullptr)
                break;
            bytes = current->sz;
        }
    }
    return 0;
}

static int l_str2p(lua_State* L)
{
    size_t sz = 0;
    const char* str = luaL_checklstring(L, 1, &sz);
    void* ptr = new char[sz];
    ::memcpy(ptr, str, sz);
    lua_pushlightuserdata(L, ptr);
    lua_pushinteger(L, (int)sz);
    
    return 2;
}

//
// for skynet socket
//

/*
    lightuserdata msg
    integer size

    return type n1 n2 ptr_or_string
*/
static int l_unpack(lua_State* L)
{
    skynet::skynet_socket_message* message = (skynet::skynet_socket_message*)lua_touserdata(L,1);
    int size = luaL_checkinteger(L,2);

    lua_pushinteger(L, message->socket_event);
    lua_pushinteger(L, message->socket_id);
    lua_pushinteger(L, message->ud);
    if (message->buffer == nullptr)
    {
        lua_pushlstring(L, (char *)(message+1),size - sizeof(*message));
    }
    else
    {
        lua_pushlightuserdata(L, message->buffer);
    }
    if (message->socket_event == skynet::skynet_socket_event::EVENT_UDP)
    {
        int addrsz = 0;
        const char* addrstring = node_socket::instance()->udp_address(message, &addrsz);
        if (addrstring != nullptr)
        {
            lua_pushlstring(L, addrstring, addrsz);
            return 5;
        }
    }

    return 4;
}

static const char* address_port(lua_State* L, char *tmp, const char* addr, int port_index, int *port)
{
    const char* host;
    if (lua_isnoneornil(L, port_index))
    {
        host = ::strchr(addr, '[');
        if (host != nullptr)
        {
            // is ipv6
            ++host;
            const char* sep = strchr(addr,']');
            if (sep == nullptr)
            {
                luaL_error(L, "Invalid address %s.", addr);
            }
            memcpy(tmp, host, sep-host);
            tmp[sep-host] = '\0';
            host = tmp;
            sep = strchr(sep + 1, ':');
            if (sep == nullptr)
            {
                luaL_error(L, "Invalid address %s.", addr);
            }
            *port = strtoul(sep+1,nullptr,10);
        }
        else
        {
            // is ipv4
            const char* sep = strchr(addr,':');
            if (sep == nullptr)
            {
                luaL_error(L, "Invalid address %s.",addr);
            }
            memcpy(tmp, addr, sep-addr);
            tmp[sep-addr] = '\0';
            host = tmp;
            *port = strtoul(sep+1,nullptr,10);
        }
    }
    else
    {
        host = addr;
        *port = luaL_optinteger(L,port_index, 0);
    }
    
    return host;
}

static int l_connect(lua_State* L)
{
    size_t sz = 0;
    const char* addr = luaL_checklstring(L, 1, &sz);
    char tmp[sz];
    int port = 0;
    const char* host = address_port(L, tmp, addr, 2, &port);
    if (port == 0)
    {
        return luaL_error(L, "Invalid port");
    }
    
    skynet::service_context* ctx = (skynet::service_context*)lua_touserdata(L, lua_upvalueindex(1));
    int id = node_socket::instance()->connect(ctx, host, port);
    lua_pushinteger(L, id);

    return 1;
}

static int l_close(lua_State* L)
{
    int id = luaL_checkinteger(L,1);
    skynet::service_context* ctx = (skynet::service_context*)lua_touserdata(L, lua_upvalueindex(1));
    node_socket::instance()->close(ctx, id);
    return 0;
}

static int l_shutdown(lua_State* L)
{
    int id = luaL_checkinteger(L,1);
    skynet::service_context* ctx = (skynet::service_context*)lua_touserdata(L, lua_upvalueindex(1));
    node_socket::instance()->shutdown(ctx, id);
    
    return 0;
}

static int l_listen(lua_State* L)
{
    const char* host = luaL_checkstring(L, 1);
    int port = luaL_checkinteger(L, 2);
    int backlog = luaL_optinteger(L, 3, BACKLOG);
    skynet::service_context* ctx = (skynet::service_context*)lua_touserdata(L, lua_upvalueindex(1));
    int id = node_socket::instance()->listen(ctx, host, port, backlog);
    if (id < 0)
    {
        return luaL_error(L, "Listen error");
    }

    lua_pushinteger(L, id);
    
    return 1;
}

static size_t count_size(lua_State* L, int index)
{
    size_t tlen = 0;
    for (int i = 1; lua_geti(L, index, i) != LUA_TNIL; ++i)
    {
        size_t len;
        luaL_checklstring(L, -1, &len);
        tlen += len;
        lua_pop(L,1);
    }
    lua_pop(L,1);
    
    return tlen;
}

static void concat_table(lua_State* L, int index, void *buffer, size_t tlen)
{
    char* ptr = (char*)buffer;
    for (int i = 1; lua_geti(L, index, i) != LUA_TNIL; ++i)
    {
        size_t len;
        const char* str = lua_tolstring(L, -1, &len);
        if (str == nullptr || tlen < len)
            break;

        ::memcpy(ptr, str, len);
        ptr += len;
        tlen -= len;
        lua_pop(L, 1);
    }
    if (tlen != 0)
    {
        delete[] buffer;
        luaL_error(L, "Invalid strings table");
    }
    
    lua_pop(L,1);
}

static void get_buffer(lua_State* L, int index, socket::send_buffer* buf)
{
    void* buffer;
    switch(lua_type(L, index))
    {
        size_t len;
    case LUA_TUSERDATA:
        // lua full useobject must be a raw pointer, it can't be a socket object or a memory object.
        buf->type = socket::buffer_type::RAW_POINTER;
        buf->buffer = lua_touserdata(L, index);
        if (lua_isinteger(L, index+1))
        {
            buf->sz = lua_tointeger(L, index+1);
        }
        else
        {
            buf->sz = lua_rawlen(L, index);
        }
        break;
    case LUA_TLIGHTUSERDATA:
    {
        int sz = -1;
        if (lua_isinteger(L, index+1))
        {
            sz = lua_tointeger(L,index+1);
        }
        if (sz < 0)
        {
            buf->type = socket::buffer_type::OBJECT;
        }
        else
        {
            buf->type = socket::buffer_type::MEMORY;
        }
        buf->buffer = lua_touserdata(L,index);
        buf->sz = (size_t)sz;
        break;
        }
    case LUA_TTABLE:
        // concat the table as a string
        len = count_size(L, index);
        buffer = new char[len];
        concat_table(L, index, buffer, len);
        buf->type = socket::buffer_type::MEMORY;
        buf->buffer = buffer;
        buf->sz = len;
        break;
    default:
        buf->type = socket::buffer_type::RAW_POINTER;
        buf->buffer = luaL_checklstring(L, index, &buf->sz);
        break;
    }
}

static int l_send(lua_State* L)
{
    skynet::service_context* ctx = (skynet::service_context*)lua_touserdata(L, lua_upvalueindex(1));
    int id = luaL_checkinteger(L, 1);
    socket::send_buffer buf;
    buf.socket_id = id;
    get_buffer(L, 2, &buf);
    int err = node_socket::instance()->sendbuffer(ctx, &buf);
    lua_pushboolean(L, !err);
    return 1;
}

static int l_send_low(lua_State* L)
{
    skynet::service_context* ctx = (skynet::service_context*)lua_touserdata(L, lua_upvalueindex(1));
    int id = luaL_checkinteger(L, 1);
    socket::send_buffer buf;
    buf.socket_id = id;
    get_buffer(L, 2, &buf);
    int err = node_socket::instance()->sendbuffer_lowpriority(ctx, &buf);
    lua_pushboolean(L, !err);
    return 1;
}

static int l_bind(lua_State* L)
{
    skynet::service_context* ctx = (skynet::service_context*)lua_touserdata(L, lua_upvalueindex(1));
    int fd = luaL_checkinteger(L, 1);
    int id = node_socket::instance()->bind(ctx,fd);
    lua_pushinteger(L,id);
    return 1;
}

static int l_start(lua_State* L)
{
    skynet::service_context* ctx = (skynet::service_context*)lua_touserdata(L, lua_upvalueindex(1));
    int id = luaL_checkinteger(L, 1);
    node_socket::instance()->start(ctx,id);
    return 0;
}

static int l_nodelay(lua_State* L)
{
    skynet::service_context* ctx = (skynet::service_context*)lua_touserdata(L, lua_upvalueindex(1));
    int id = luaL_checkinteger(L, 1);
    node_socket::instance()->nodelay(ctx,id);
    return 0;
}

static int l_udp(lua_State* L)
{
    skynet::service_context* ctx = (skynet::service_context*)lua_touserdata(L, lua_upvalueindex(1));
    size_t sz = 0;
    const char* addr = lua_tolstring(L,1,&sz);
    char tmp[sz];
    int port = 0;
    const char* host = nullptr;
    if (addr)
    {
        host = address_port(L, tmp, addr, 2, &port);
    }

    int id = node_socket::instance()->udp(ctx, host, port);
    if (id < 0)
    {
        return luaL_error(L, "udp init failed");
    }
    lua_pushinteger(L, id);
    return 1;
}

static int l_udp_connect(lua_State* L)
{
    skynet::service_context* ctx = (skynet::service_context*)lua_touserdata(L, lua_upvalueindex(1));
    int id = luaL_checkinteger(L, 1);
    size_t sz = 0;
    const char* addr = luaL_checklstring(L,2,&sz);
    char tmp[sz];
    int port = 0;
    const char* host = nullptr;
    if (addr)
    {
        host = address_port(L, tmp, addr, 3, &port);
    }

    if (node_socket::instance()->udp_connect(ctx, id, host, port))
    {
        return luaL_error(L, "udp connect failed");
    }

    return 0;
}

static int l_udp_send(lua_State* L)
{
    // skynet::service_context* ctx = (skynet::service_context*)lua_touserdata(L, lua_upvalueindex(1));
    // int id = luaL_checkinteger(L, 1);
    // const char* address = luaL_checkstring(L, 2);
    // socket::send_buffer buf;
    // buf.socket_id = id;
    // get_buffer(L, 3, &buf);
    // int err = node_socket::instance()->udp_sendbuffer(ctx, address, &buf);

    // lua_pushboolean(L, !err);

    return 1;
}

static int l_udp_address(lua_State* L)
{
    size_t sz = 0;
    const uint8_t * addr = (const uint8_t *)luaL_checklstring(L, 1, &sz);
    uint16_t port = 0;
    memcpy(&port, addr+1, sizeof(uint16_t));
    port = ntohs(port);
    const void * src = addr+3;
    char tmp[256];
    int family;
    if (sz == 1+2+4)
    {
        family = AF_INET;
    }
    else
    {
        if (sz != 1+2+16)
        {
            return luaL_error(L, "Invalid udp address");
        }
        family = AF_INET6;
    }
    if (inet_ntop(family, src, tmp, sizeof(tmp)) == nullptr)
    {
        return luaL_error(L, "Invalid udp address");
    }
    lua_pushstring(L, tmp);
    lua_pushinteger(L, port);
    return 2;
}

static void getinfo(lua_State* L, socket::socket_info* si)
{
    lua_newtable(L);
    lua_pushinteger(L, si->socket_id);
    lua_setfield(L, -2, "id");
    lua_pushinteger(L, si->svc_handle);
    lua_setfield(L, -2, "address");
    switch(si->status)
    {
    case socket::socket_info::status::LISTEN:
        lua_pushstring(L, "LISTEN");
        lua_setfield(L, -2, "type");
        lua_pushinteger(L, si->recv);
        lua_setfield(L, -2, "accept");
        lua_pushinteger(L, si->recv_time);
        lua_setfield(L, -2, "rtime");
        if (si->endpoint[0])
        {
            lua_pushstring(L, si->endpoint);
            lua_setfield(L, -2, "sock");
        }
        return;
    case socket::socket_info::status::TCP:
        lua_pushstring(L, "TCP");
        break;
    case socket::socket_info::status::UDP:
        lua_pushstring(L, "UDP");
        break;
    case socket::socket_info::status::BIND:
        lua_pushstring(L, "BIND");
        break;
    default:
        lua_pushstring(L, "UNKNOWN");
        lua_setfield(L, -2, "type");
        return;
    }
    lua_setfield(L, -2, "type");
    lua_pushinteger(L, si->recv);
    lua_setfield(L, -2, "read");
    lua_pushinteger(L, si->send);
    lua_setfield(L, -2, "write");
    lua_pushinteger(L, si->wb_size);
    lua_setfield(L, -2, "wbuffer");
    lua_pushinteger(L, si->recv_time);
    lua_setfield(L, -2, "rtime");
    lua_pushinteger(L, si->send_time);
    lua_setfield(L, -2, "wtime");
    if (si->endpoint[0])
    {
        lua_pushstring(L, si->endpoint);
        lua_setfield(L, -2, "peer");
    }
}

static int l_info(lua_State* L)
{
    lua_newtable(L);
    socket::socket_info* si = node_socket::instance()->get_socket_info();
    socket::socket_info* temp = si;
    int n = 0;
    while (temp)
    {
        getinfo(L, temp);
        lua_seti(L, -2, ++n);
        temp = temp->next;
    }
    socket::socket_info::release(si);
    return 1;
}

} }

LUAMOD_API int luaopen_skynet_socketdriver(lua_State* L)
{
    luaL_checkversion(L);

    luaL_Reg l[] = {
        { "buffer", skynet::luaclib::l_newbuffer },
        { "push", skynet::luaclib::l_pushbuffer },
        { "pop", skynet::luaclib::l_popbuffer },
        { "drop", skynet::luaclib::l_drop },
        { "readall", skynet::luaclib::l_readall },
        { "clear", skynet::luaclib::lclearbuffer },
        { "readline", skynet::luaclib::l_readline },
        { "str2p", skynet::luaclib::l_str2p },
        { "header", skynet::luaclib::l_header },
        { "info", skynet::luaclib::l_info },

        { "unpack", skynet::luaclib::l_unpack },

        { nullptr, nullptr },
    };
    luaL_newlib(L, l);
    
    luaL_Reg l2[] = {
        { "connect", skynet::luaclib::l_connect },
        { "close", skynet::luaclib::l_close },
        { "shutdown", skynet::luaclib::l_shutdown },
        { "listen", skynet::luaclib::l_listen },
        { "send", skynet::luaclib::l_send },
        { "lsend", skynet::luaclib::l_send_low },
        { "bind", skynet::luaclib::l_bind },
        { "start", skynet::luaclib::l_start },
        { "nodelay", skynet::luaclib::l_nodelay },
        { "udp", skynet::luaclib::l_udp },
        { "udp_connect", skynet::luaclib::l_udp_connect },
        // { "udp_send", skynet::luaclib::l_udp_send },
        { "udp_address", skynet::luaclib::l_udp_address },

        { nullptr, nullptr },
    };

    //
    lua_getfield(L, LUA_REGISTRYINDEX, "service_context");
    skynet::service_context* ctx = (skynet::service_context*)lua_touserdata(L, -1);
    if (ctx == nullptr)
    {
        return luaL_error(L, "Init skynet service context first");
    }

    luaL_setfuncs(L, l2, 1);

    return 1;
}
