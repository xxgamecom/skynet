#pragma once

struct lua_State;

namespace skynet { namespace luaclib {

// skynet.socketdriver
class skynet_socket final
{
    // without service_context upvalue
public:
    static int l_new_socket_buffer(lua_State* L);
    static int l_push_socket_buffer(lua_State* L);
    static int l_pop_socket_buffer(lua_State* L);
    static int l_clear_socket_buffer(lua_State* L);
    static int l_drop(lua_State* L);
    static int l_read_all(lua_State* L);
    static int l_read_line(lua_State* L);
    static int l_header(lua_State* L);
    static int l_str2p(lua_State* L);
    static int l_query_socket_info(lua_State* L);
    static int l_unpack(lua_State* L);

    // need service_context upvalue
public:
    static int l_connect(lua_State* L);
    static int l_close(lua_State* L);
    static int l_shutdown(lua_State* L);
    static int l_listen(lua_State* L);
    static int l_send(lua_State* L);
    static int l_send_low(lua_State* L);
    static int l_bind(lua_State* L);
    static int l_start(lua_State* L);
    static int l_pause(lua_State* L);
    static int l_nodelay(lua_State* L);
    static int l_udp(lua_State* L);
    static int l_udp_connect(lua_State* L);
    static int l_udp_send(lua_State* L);
    static int l_udp_address(lua_State* L);
};

} }
