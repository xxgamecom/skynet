#pragma  once

// forward declear
struct lua_State;

namespace skynet { namespace luaclib {

class skynet_core final
{
    // service_context functions
public:
    // send the message to service
    static int l_send(lua_State* L);
    // redirect the message to service
    static int l_redirect(lua_State* L);

    // exec service command
    static int l_service_command(lua_State* L);
    // exec service command
    static int l_service_command_int(lua_State* L);
    // exec service command (address related)
    static int l_service_command_address(lua_State* L);

    // set service message callback
    static int l_set_service_callback(lua_State* L);
    //
    static int l_gen_session_id(lua_State* L);

    static int l_log(lua_State* L);
    static int l_trace(lua_State* L);

    // functions without service_context
public:
    static int l_tostring(lua_State* L);
    static int l_pack(lua_State* L);
    static int l_unpack(lua_State* L);
    static int l_pack_string(lua_State* L);
    static int l_trash(lua_State* L);
    static int l_now(lua_State* L);
    static int l_hpc(lua_State* L);
};

} }
