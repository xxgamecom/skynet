#define LUA_LIB

#include "skynet.h"
#include "lua-seri.h"

extern "C" {
#include <lua.h>
#include <lauxlib.h>
}

#include <cstdlib>
#include <cassert>
#include <cinttypes>

//
#define KNRM  "\x1B[0m"
#define KRED  "\x1B[31m"

//
static const char* _get_dst_svc_handle_string(lua_State* L, int index)
{
    const char* dst_svc_handle_string = lua_tostring(L, index);
    if (dst_svc_handle_string == nullptr)
    {
        luaL_error(L, "dest address type (%s) must be a string or number.", lua_typename(L, lua_type(L, index)));
    }

    return dst_svc_handle_string;
}

/**
 * send message to service
 *
 * @param src_svc_handle source service handle, 0 means self.
 * @param message_type_idx the stack index of message protocol type
 * @return 0 , 1
 */
static int _send_message(lua_State* L, int src_svc_handle, int message_type_idx)
{
    auto svc_ctx = (skynet::service_context*)lua_touserdata(L, lua_upvalueindex(1));

    // arg 1 - destination service handle (integer | string)
    uint32_t dst_svc_handle = (uint32_t)lua_tointeger(L, 1);
    const char* dst_svc_handle_string = nullptr;
    if (dst_svc_handle == 0)
    {
        if (lua_type(L, 1) == LUA_TNUMBER)
        {
            return luaL_error(L, "Invalid destination service handle: 0");
        }
        dst_svc_handle_string = _get_dst_svc_handle_string(L, 1);
    }

    // arg 2 - protocol type
    int protocol_type = luaL_checkinteger(L, message_type_idx + 0);

    // arg 3 - session id, 0 means need alloc a new session id
    int session_id = 0;
    if (lua_isnil(L, message_type_idx + 1))
        protocol_type |= MESSAGE_TAG_ALLOC_SESSION;
    else
        session_id = luaL_checkinteger(L, message_type_idx + 1);

    // arg 4 - message (string | lightuserdata)
    int msg_type = lua_type(L, message_type_idx + 2);
    switch (msg_type)
    {
    case LUA_TSTRING:
    {
        size_t len = 0;
        const char* msg = lua_tolstring(L, message_type_idx + 2, &len);
        if (len == 0)
        {
            msg = nullptr;
        }

        if (dst_svc_handle_string != nullptr)
            session_id = skynet::service_manager::instance()->send_by_name(svc_ctx, src_svc_handle, dst_svc_handle_string, protocol_type, session_id, (void*)msg, len);
        else
            session_id = skynet::service_manager::instance()->send(svc_ctx, src_svc_handle, dst_svc_handle, protocol_type, session_id, (void*)msg, len);
        break;
    }
    case LUA_TLIGHTUSERDATA:
    {
        void* msg = lua_touserdata(L, message_type_idx + 2);
        int size = luaL_checkinteger(L, message_type_idx + 3);
        if (dst_svc_handle_string != nullptr)
            session_id = skynet::service_manager::instance()->send_by_name(svc_ctx, src_svc_handle, dst_svc_handle_string, protocol_type | MESSAGE_TAG_DONT_COPY, session_id, msg, size);
        else
            session_id = skynet::service_manager::instance()->send(svc_ctx, src_svc_handle, dst_svc_handle, protocol_type | MESSAGE_TAG_DONT_COPY, session_id, msg, size);
        break;
    }
    default:
        luaL_error(L, "Invalid param %s", lua_typename(L, lua_type(L, message_type_idx + 2)));
        break;
    }

    //
    if (session_id < 0)
    {
        // package is too large
        if (session_id == -2)
        {
            lua_pushboolean(L, 0);
            return 1;
        }

        // send to invalid address, todo: maybe throw an error would be better
        return 0;
    }
    lua_pushinteger(L, session_id);

    return 1;
}

/**
 * send the message to service
 *
 * arguments:
 * 1 destination service handle  - uint32 | string
 * 2 message protocol type       - integer
 * 3 session                     - integer
 * 4 message                     - string | lightuserdata (message_ptr, integer len)
 *
 * outputs:
 * session id                    - integer
 *
 * lua examples:
 * c.send(addr, proto.id, session, proto.pack(...))
 * ...
 */
static int l_send(lua_State* L)
{
    return _send_message(L, 0, 2);
}

/**
 * redirect the message to service
 *
 * arguments:
 * 1 destination service handle  - uint32 | string
 * 2 source service handle       - integer
 * 3 message protocol type       - integer
 * 4 session                     - integer
 * 5 message                     - string | lightuserdata (message_ptr, integer len)
 *
 * lua examples:
 * c.redirect(dest, source, proto.id, ...)
 * ...
 */
static int l_redirect(lua_State* L)
{
    auto src_svc_handle = (uint32_t)luaL_checkinteger(L, 2);
    return _send_message(L, src_svc_handle, 3);
}

/**
 * exec service command
 *
 * arguments:
 * 1 service cmd        - string
 * 2 service cmd param  - string
 *
 * lua examples:
 * c.command("KILL", name)
 * c.command("LAUNCH", table.concat({...}, " "))
 * ...
 */
static int l_service_command(lua_State* L)
{
    auto svc_ctx = (skynet::service_context*)lua_touserdata(L, lua_upvalueindex(1));

    // cmd
    const char* cmd = luaL_checkstring(L, 1);

    // cmd param
    const char* cmd_param = nullptr;
    if (lua_gettop(L) == 2)
    {
        cmd_param = luaL_checkstring(L, 2);
    }

    // handle service command
    const char* result = skynet::service_command::exec(svc_ctx, cmd, cmd_param);
    if (result == nullptr)
        return 0;

    // return result
    lua_pushstring(L, result);

    return 1;
}

/**
 * exec service command
 *
 * arguments:
 * 1 service cmd        - string
 * 2 service cmd param  - string
 *
 * lua examples:
 * c.intcommand("TIMEOUT", timeout)
 * c.intcommand("STAT", "mqlen")
 * ...
 */
static int l_service_command_int(lua_State* L)
{
    // service_context upvalue
    auto svc_ctx = (skynet::service_context*)lua_touserdata(L, lua_upvalueindex(1));

    // cmd
    const char* cmd = luaL_checkstring(L, 1);

    // cmd param
    std::string cmd_param;
    if (lua_gettop(L) == 2)
    {
        if (lua_isnumber(L, 2))
        {
            cmd_param = std::to_string((int32_t)luaL_checkinteger(L, 2));
        }
        else
        {
            cmd_param = luaL_checkstring(L, 2);
        }
    }

    // exec service command
    const char* result = skynet::service_command::exec(svc_ctx, cmd, cmd_param.c_str());
    if (result == nullptr)
        return 0;

    // return result
    char* endptr = nullptr;
    lua_Integer r = ::strtoll(result, &endptr, 0);
    if (endptr == nullptr || *endptr != '\0')
    {
        // may be real number
        double n = ::strtod(result, &endptr);
        if (endptr == nullptr || *endptr != '\0')
        {
            return luaL_error(L, "Invalid service cmd result %s", result);
        }

        lua_pushnumber(L, n);
    }
    else
    {
        lua_pushinteger(L, r);
    }

    return 1;
}

/**
 * exec service command (address related)
 *
 * arguments:
 * 1 service cmd        - string
 * 2 service cmd param  - string
 *
 * examples:
 * c.addresscommand("REG")
 * c.addresscommand("QUERY", name)
 */
static int l_service_command_address(lua_State* L)
{
    // service context upvalue
    auto svc_ctx = (skynet::service_context*)lua_touserdata(L, lua_upvalueindex(1));

    // cmd
    const char* cmd = luaL_checkstring(L, 1);

    // cmd param
    std::string cmd_param;
    if (lua_gettop(L) == 2)
    {
        cmd_param = luaL_checkstring(L, 2);
    }

    // exec service command
    const char* result = skynet::service_command::exec(svc_ctx, cmd, cmd_param.c_str());
    if (result == nullptr || result[0] != ':')
        return 0;

    // return result
    uint32_t addr = 0;
    for (int i = 1; result[i]; i++)
    {
        int c = result[i];
        if (c >= '0' && c <= '9')
        {
            c = c - '0';
        }
        else if (c >= 'a' && c <= 'f')
        {
            c = c - 'a' + 10;
        }
        else if (c >= 'A' && c <= 'F')
        {
            c = c - 'A' + 10;
        }
        else
        {
            return 0;
        }
        addr = addr * 16 + c;
    }
    lua_pushinteger(L, addr);

    return 1;
}

static int _traceback(lua_State* L)
{
    const char* msg = lua_tostring(L, 1);
    if (msg != nullptr)
    {
        luaL_traceback(L, L, msg, 1);
    }
    else
    {
        lua_pushliteral(L, "(no error message)");
    }

    return 1;
}

/**
 * service message callback
 * 最终会调用Lua层的dispatch_message，参数依次是：type, msg, sz, session, src_svc_handle。
 * 所以，snlua类型的服务收到消息时最终会调用Lua层的消息回调函数 skynet.handle_service_message。
 *
 * @return 0 need delete msg, 1 don't delete msg
 */
static int _cb(skynet::service_context* svc_ctx, void* ud, int type, int session, uint32_t src_svc_handle, const void* msg, size_t sz)
{
    lua_State* L = (lua_State*)ud;

    int top = lua_gettop(L);
    if (top == 0)
    {
        lua_pushcfunction(L, _traceback);
        lua_rawgetp(L, LUA_REGISTRYINDEX, (void*)_cb);
    }
    else
    {
        assert(top == 2);
    }
    lua_pushvalue(L, 2);

    lua_pushinteger(L, type);
    lua_pushlightuserdata(L, (void*)msg);
    lua_pushinteger(L, sz);
    lua_pushinteger(L, session);
    lua_pushinteger(L, src_svc_handle);

    int trace = 1;
    int r = lua_pcall(L, 5, 0, trace);
    if (r == LUA_OK)
    {
        return 0;
    }

    const char* self = skynet::service_command::exec(svc_ctx, "REG");
    switch (r)
    {
    case LUA_ERRRUN:
        log(svc_ctx, "lua call [%x to %s : %d msgsz = %d] error : " KRED "%s" KNRM, src_svc_handle, self, session, sz, lua_tostring(L, -1));
        break;
    case LUA_ERRMEM:
        log(svc_ctx, "lua memory error : [%x to %s : %d]", src_svc_handle, self, session);
        break;
    case LUA_ERRERR:
        log(svc_ctx, "lua error in error : [%x to %s : %d]", src_svc_handle, self, session);
        break;
    case LUA_ERRGCMM:
        log(svc_ctx, "lua gc error : [%x to %s : %d]", src_svc_handle , self, session);
        break;
    };

    lua_pop(L, 1);

    return 0;
}

/**
 * callback in forward mode
 *
 * @return always return 1, means don't delete msg
 */
static int _forward_cb(skynet::service_context* svc_ctx, void* ud, int type, int session, uint32_t src_svc_handle, const void* msg, size_t sz)
{
    _cb(svc_ctx, ud, type, session, src_svc_handle, msg, sz);

    // don't delete msg in forward mode.
    return 1;
}

/**
 * set service message callback
 *
 * arguments:
 * 1 callback function      - function
 * 2 message forward mode   - boolean
 *
 * lua examples:
 * c.callback(skynet.handle_service_message)
 * c.callback(function(ptype, msg, sz, ...) ... end, true)
 */
static int l_set_service_callback(lua_State* L)
{
    // service context upvalue
    auto svc_ctx = (skynet::service_context*)lua_touserdata(L, lua_upvalueindex(1));

    // forward mode check (arg 2)
    int forward = lua_toboolean(L, 2);

    // callback function check (must function) (arg 1)
    luaL_checktype(L, 1, LUA_TFUNCTION);

    // add _cb to global register table
    lua_settop(L, 1);
    lua_rawsetp(L, LUA_REGISTRYINDEX, (const void*)_cb);

    // push vm main thread to stack top & get lua thread
    lua_rawgeti(L, LUA_REGISTRYINDEX, LUA_RIDX_MAINTHREAD);
    lua_State* gL = lua_tothread(L, -1);

    // forward mode
    if (forward)
    {
        svc_ctx->set_callback(_forward_cb, gL);
    }
    else
    {
        svc_ctx->set_callback(_cb, gL);
    }

    return 0;
}

/**
 * generate a new session id
 *
 * lua examples:
 * c.gen_session_id()
 */
static int l_gen_session_id(lua_State* L)
{
    // service context upvalue
    auto svc_ctx = (skynet::service_context*)lua_touserdata(L, lua_upvalueindex(1));

    // gen session id
    int session_id = skynet::service_manager::instance()->send(svc_ctx, 0, 0, MESSAGE_TAG_ALLOC_SESSION, 0, nullptr, 0);

    // return session id
    lua_pushinteger(L, session_id);
    return 1;
}

/**
 * log
 *
 * arguments:
 * args num <= 1
 * 1 log message    - string
 * args num > 1
 * 1
 *
 * lua examples:
 * skynet.log("Server start")
 * skynet.log(string.format("socket accept from %s", msg))
 * skynet.log(addr, "connected")
 * ...
 */
static int l_log(lua_State* L)
{
    // service context upvalue
    auto svc_ctx = (skynet::service_context*)lua_touserdata(L, lua_upvalueindex(1));

    //
    const char* log_msg = nullptr;

    int n = lua_gettop(L);
    if (n <= 1)
    {
        lua_settop(L, 1);
        log_msg = luaL_tolstring(L, 1, nullptr);
    }
    else
    {
        //
        luaL_Buffer b;
        luaL_buffinit(L, &b);
        for (int i = 1; i <= n; i++)
        {
            // convert to string and push to stack top
            luaL_tolstring(L, i, nullptr);
            // add to buffer
            luaL_addvalue(&b);

            // add seperate char ' '
            if (i < n)
            {
                luaL_addchar(&b, ' ');
            }
        }

        // end buffer, push the string to stack top
        luaL_pushresult(&b);
        log_msg = lua_tostring(L, -1);
    }

    // do log
    if (log_msg != nullptr)
    {
        log(svc_ctx, "%s", log_msg);
    }

    return 0;
}

/**
 * debug trace, output trace message
 *
 * arguments:
 * 1 tag            - string
 * 2 user string    - string
 * 3 co             - thread, default nil | current L
 * 4 trace level    - integer, default nil
 *
 * lua examples:
 * c.trace(tag, "error")
 * c.trace(tag, "call", 2)
 */

#define MAX_TRACE_LEVEL 3

struct source_info
{
    const char* source;
    int line;
};

static int l_trace(lua_State* L)
{
    //
    auto svc_ctx = (skynet::service_context*)lua_touserdata(L, lua_upvalueindex(1));

    //
    const char* tag = luaL_checkstring(L, 1);
    const char* user = luaL_checkstring(L, 2);

    // only 2 arguments
    if (lua_isnoneornil(L, 3))
    {
        log(svc_ctx, "<TRACE %s> %lld %s", tag, skynet::time_helper::get_time_ns(), user);
        return 0;
    }

    // arg 3, 4: co, level
    lua_State* co = L;
    int level;
    if (lua_isthread(L, 3))
    {
        co = lua_tothread(L, 3);
        level = luaL_optinteger(L, 4, 1);
    }
    else
    {
        level = luaL_optinteger(L, 3, 1);
    }

    //
    struct source_info si[MAX_TRACE_LEVEL];
    lua_Debug d;
    int index = 0;
    do
    {
        // get call stack info
        if (lua_getstack(co, level, &d) == 0)
            break;

        // get call info
        lua_getinfo(co, "Sl", &d);
        level++;
        si[index].source = d.source;
        si[index].line = d.currentline;
        if (d.currentline >= 0)
            ++index;
    } while (index < MAX_TRACE_LEVEL);

    //
    switch (index)
    {
    case 1:
        log(svc_ctx, "<TRACE %s> %lld %s : %s:%d", tag, skynet::time_helper::get_time_ns(), user, si[0].source, si[0].line);
        break;
    case 2:
        log(svc_ctx, "<TRACE %s> %lld %s : %s:%d %s:%d", tag, skynet::time_helper::get_time_ns(), user,
            si[0].source, si[0].line, si[1].source, si[1].line);
        break;
    case 3:
        log(svc_ctx, "<TRACE %s> %lld %s : %s:%d %s:%d %s:%d", tag, skynet::time_helper::get_time_ns(), user,
            si[0].source, si[0].line, si[1].source, si[1].line, si[2].source, si[2].line);
        break;
    default:
        log(svc_ctx, "<TRACE %s> %lld %s", tag, skynet::time_helper::get_time_ns(), user);
        break;
    }

    return 0;
}

//----------------------------------------------
// without service_context
//----------------------------------------------

/**
 * convert userdata to string, TODO: change name to l_unpack ?
 *
 * arguments:
 * 1 message            - userdata
 * 2 message size       - integer
 *
 * lua examples:
 * 1) c.tostring(msg, sz)
 * 2) skynet.tostring = assert(c.tostring)
 * 3) skynet.register_svc_msg_handler {
 *        unpack = skynet.tostring,
 *        ...
 *    }
 */
static int l_tostring(lua_State* L)
{
    // check first arguments
    if (lua_isnoneornil(L, 1) != 0)
        return 0;

    //
    char* msg = (char*)lua_touserdata(L, 1);
    int msg_sz = luaL_checkinteger(L, 2);
    lua_pushlstring(L, msg, msg_sz);

    return 1;
}

/**
 *
 */
static int l_pack(lua_State* L)
{
    return luaseri_pack(L);
}

/**
 *
 */
static int l_unpack(lua_State* L)
{
    return luaseri_unpack(L);
}

/**
 * serialize object to string
 *
 * arguments:
 * 1 obj            - userdata
 * 2
 *
 * outputs:
 * 1 string
 *
 * lua examples:
 * 1) msg = skynet.pack_string(addr)
 * 2) skynet.pack_string = assert(c.pack_string)
 * 3) function pack_package(...)
 *        local message = skynet.pack_string(...)
 *        local size = #message
 *        assert(size <= 255 , "too long")
 *        return string.char(size) .. message
 *    end
 */
static int l_pack_string(lua_State* L)
{
    luaseri_pack(L);

    char* str = (char*)lua_touserdata(L, -2);
    int sz = lua_tointeger(L, -1);
    lua_pushlstring(L, str, sz);

    // TODO: delete
    skynet_free(str);

    return 1;
}

/**
 * release message
 *
 * arguments:
 * 1 message        - string | lightuserdata
 * 2 message size   - integer
 *
 * lua examples:
 * skynet.trash = assert(c.trash)
 * c.trash(msg, sz)
 */
static int l_trash(lua_State* L)
{
    int t = lua_type(L, 1);
    if (t == LUA_TSTRING)
        return 0;

    if (t == LUA_TLIGHTUSERDATA)
    {
        char* msg = (char*)lua_touserdata(L, 1);
        luaL_checkinteger(L, 2);
        delete[] msg;
    }
    else
    {
        luaL_error(L, "skynet.trash invalid param %s", lua_typename(L, t));
    }

    return 0;
}

/**
 * get the number of tick since skynet node stared
 *
 * outputs:
 * integer
 *
 * lua examples:
 * skynet.now = c.now
 */
static int l_now(lua_State* L)
{
    lua_pushinteger(L, skynet::timer_manager::instance()->now());
    return 1;
}

/**
 * high performance counter (nanoseconds)
 *
 * outputs:
 * integer
 *
 * lua examples:
 * skynet.hpc = c.hpc
 */
static int l_hpc(lua_State* L)
{
    lua_pushinteger(L, skynet::time_helper::get_time_ns());
    return 1;
}

/**
 * skynet luaclib - skynet.core
 */

#if __cplusplus
extern "C" {
#endif

LUAMOD_API int luaopen_skynet_core(lua_State* L)
{
    luaL_checkversion(L);

    // need service_context upvalue
    luaL_Reg core_funcs_1[] = {
        { "send",           l_send },
        { "redirect",       l_redirect },
        { "command",        l_service_command },
        { "intcommand",     l_service_command_int },
        { "addresscommand", l_service_command_address },
        { "callback",       l_set_service_callback },
        { "gen_session_id", l_gen_session_id },
        { "log",            l_log },
        { "trace",          l_trace },

        { nullptr,          nullptr },
    };

    // without service_context upvalue
    luaL_Reg core_funcs_2[] = {
        { "tostring",    l_tostring },
        { "pack",        l_pack },
        { "unpack",      l_unpack },
        { "pack_string", l_pack_string },
        { "trash",       l_trash },
        { "now",         l_now },
        { "hpc",         l_hpc },

        { nullptr,       nullptr },
    };

    lua_createtable(L, 0, sizeof(core_funcs_1) / sizeof(core_funcs_1[0]) + sizeof(core_funcs_2) / sizeof(core_funcs_2[0]) - 2);

    // get service_context from global register (see: snlua_service::init_lua_cb()), push it to stack top.
    // can use it by lua_upvalueindex(1)
    lua_getfield(L, LUA_REGISTRYINDEX, "service_context");
    auto svc_ctx = (skynet::service_context*)lua_touserdata(L, -1);
    if (svc_ctx == nullptr)
    {
        return luaL_error(L, "[skynet.core] Init skynet service context first");
    }
    // with service_context upvalue
    luaL_setfuncs(L, core_funcs_1, 1);

    // without service_context
    luaL_setfuncs(L, core_funcs_2, 0);

    return 1;
}

#if __cplusplus
}
#endif
