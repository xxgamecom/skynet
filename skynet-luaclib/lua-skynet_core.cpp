#define LUA_LIB

#include "lua-skynet_core.h"

#include "skynet.h"
#include "lua-seri.h"

extern "C" {
#include <lua.h>
#include <lauxlib.h>
}

#include <cstdlib>
#include <cassert>
#include <cinttypes>

namespace skynet { namespace luaclib {

//
#define KNRM  "\x1B[0m"
#define KRED  "\x1B[31m"

#define MAX_LEVEL 3

struct source_info
{
    const char* source;
    int line;
};

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
    service_context* svc_ctx = (service_context*)lua_touserdata(L, lua_upvalueindex(1));

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
    int session = 0;
    if (lua_isnil(L, message_type_idx + 1))
        protocol_type |= MESSAGE_TAG_ALLOC_SESSION;
    else
        session = luaL_checkinteger(L, message_type_idx + 1);

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
            session = service_manager::instance()->send_by_name(svc_ctx, src_svc_handle, dst_svc_handle_string, protocol_type, session, (void*)msg, len);
        else
            session = service_manager::instance()->send(svc_ctx, src_svc_handle, dst_svc_handle, protocol_type, session, (void*)msg, len);
        break;
    }
    case LUA_TLIGHTUSERDATA:
    {
        void* msg = lua_touserdata(L, message_type_idx + 2);
        int size = luaL_checkinteger(L, message_type_idx + 3);
        if (dst_svc_handle_string != nullptr)
            session = service_manager::instance()->send_by_name(svc_ctx, src_svc_handle, dst_svc_handle_string, protocol_type | MESSAGE_TAG_DONT_COPY, session, msg, size);
        else
            session = service_manager::instance()->send(svc_ctx, src_svc_handle, dst_svc_handle, protocol_type | MESSAGE_TAG_DONT_COPY, session, msg, size);
        break;
    }
    default:
        luaL_error(L, "Invalid param %s", lua_typename(L, lua_type(L, message_type_idx + 2)));
        break;
    }

    //
    if (session < 0)
    {
        // package is too large
        if (session == -2)
        {
            lua_pushboolean(L, 0);
            return 1;
        }

        // send to invalid address, todo: maybe throw an error would be better
        return 0;
    }
    lua_pushinteger(L, session);

    return 1;
}

/**
 * send the message to service
 *
 * lua layer:
 * c.send(addr, proto.id, session, proto.pack(...))
 *
 * arguments:
 *  destination service handle  - uint32 | string
 *  message protocol type       - integer
 *  session                     - integer
 *  message                     - string | lightuserdata (message_ptr, integer len)
 */
int skynet_core::l_send(lua_State* L)
{
    return _send_message(L, 0, 2);
}

/**
 * redirect the message to service
 *
 * lua layer:
 * c.redirect(dest, source, proto.id, ...)
 *
 * arguments:
 *  destination service handle  - uint32 | string
 *  source service handle       - integer
 *  message protocol type       - integer
 *  session                     - integer
 *  message                     - string | lightuserdata (message_ptr, integer len)
 */
int skynet_core::l_redirect(lua_State* L)
{
    uint32_t src_svc_handle = (uint32_t)luaL_checkinteger(L, 2);
    return _send_message(L, src_svc_handle, 3);
}

// exec service command
int skynet_core::l_service_command(lua_State* L)
{
    service_context* svc_ctx = (service_context*)lua_touserdata(L, lua_upvalueindex(1));

    // cmd
    const char* cmd = luaL_checkstring(L, 1);

    // has 2 arguments, get the command param
    const char* param = nullptr;
    if (lua_gettop(L) == 2)
    {
        param = luaL_checkstring(L, 2);
    }

    const char* result = service_command::handle_command(svc_ctx, cmd, param);
    if (result != nullptr)
    {
        lua_pushstring(L, result);
        return 1;
    }

    return 0;
}

// exec service command
int skynet_core::l_service_command_int(lua_State* L)
{
    //
    service_context* svc_ctx = (service_context*)lua_touserdata(L, lua_upvalueindex(1));

    // get the service command
    const char* cmd = luaL_checkstring(L, 1);

    // get the service commmand param
    const char* param = nullptr;
    char tmp[64] = { 0 };    // for integer param
    if (lua_gettop(L) == 2)
    {
        if (lua_isnumber(L, 2))
        {
            int32_t n = (int32_t)luaL_checkinteger(L, 2);
            ::sprintf(tmp, "%d", n);
            param = tmp;
        }
        else
        {
            param = luaL_checkstring(L, 2);
        }
    }

    // exec service command
    const char* result = service_command::handle_command(svc_ctx, cmd, param);
    if (result != nullptr)
    {
        char* endptr = nullptr;
        lua_Integer r = ::strtoll(result, &endptr, 0);
        if (endptr == nullptr || *endptr != '\0')
        {
            // may be real number
            double n = ::strtod(result, &endptr);
            if (endptr == nullptr || *endptr != '\0')
            {
                return luaL_error(L, "Invalid result %s", result);
            }
            else
            {
                lua_pushnumber(L, n);
            }
        }
        else
        {
            lua_pushinteger(L, r);
        }

        return 1;
    }

    return 0;
}

// exec service command (address related)
int skynet_core::l_service_command_address(lua_State* L)
{
    // get the service context
    service_context* svc_ctx = (service_context*)lua_touserdata(L, lua_upvalueindex(1));

    // get command
    const char* cmd = luaL_checkstring(L, 1);

    // has 2 arguments, get the command param
    const char* param = nullptr;
    if (lua_gettop(L) == 2)
    {
        param = luaL_checkstring(L, 2);
    }

    // exec service command
    const char* result = service_command::handle_command(svc_ctx, cmd, param);
    if (result != nullptr && result[0] == ':')
    {
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

    return 0;
}

static int traceback(lua_State* L)
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
 * 最终会调用Lua层的dispatch_message，参数依次是：type, msg, sz, session, source。
 * 所以，snlua类型的服务收到消息时最终会调用Lua层的消息回调函数skynet.dispatch_message。
 *
 * @return 0 need delete msg, 1 don't delete msg
 */
static int _cb(service_context* svc_ctx, void* ud, int type, int session, uint32_t source, const void* msg, size_t sz)
{
    lua_State* L = (lua_State*)ud;

    int top = lua_gettop(L);
    if (top == 0)
    {
        lua_pushcfunction(L, traceback);
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
    lua_pushinteger(L, source);

    int trace = 1;
    int r = lua_pcall(L, 5, 0, trace);
    if (r == LUA_OK)
    {
        return 0;
    }

    const char* self = service_command::handle_command(svc_ctx, "REG", nullptr);
    switch (r)
    {
    case LUA_ERRRUN:
        log(svc_ctx, "lua call [%x to %s : %d msgsz = %d] error : " KRED "%s" KNRM, source, self, session, sz, lua_tostring(L, -1));
        break;
    case LUA_ERRMEM:
        log(svc_ctx, "lua memory error : [%x to %s : %d]", source, self, session);
        break;
    case LUA_ERRERR:
        log(svc_ctx, "lua error in error : [%x to %s : %d]", source, self, session);
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
static int _forward_cb(service_context* svc_ctx, void* ud, int type, int session, uint32_t source, const void* msg, size_t sz)
{
    _cb(svc_ctx, ud, type, session, source, msg, sz);

    // don't delete msg in forward mode.
    return 1;
}

/**
 * set service message callback
 *
 * set lua callback: skynet.dispatch_messsage(type, msg, sz, session, source)
 */
int skynet_core::l_set_service_callback(lua_State* L)
{
    //
    service_context* svc_ctx = (service_context*)lua_touserdata(L, lua_upvalueindex(1));

    // check second argument (forward mode)
    int forward = lua_toboolean(L, 2);
    // check first argument (must function)
    luaL_checktype(L, 1, LUA_TFUNCTION);

    // add _cb to global register table
    lua_settop(L, 1);
    lua_rawsetp(L, LUA_REGISTRYINDEX, (void*)_cb);

    // push vm main thread to stack top
    lua_rawgeti(L, LUA_REGISTRYINDEX, LUA_RIDX_MAINTHREAD);
    // to lua thread
    lua_State* gL = lua_tothread(L, -1);

    // forward mode
    if (forward)
    {
        svc_ctx->set_callback(gL, _forward_cb);
    }
    else
    {
        svc_ctx->set_callback(gL, _cb);
    }

    return 0;
}

// generate a new session id
int skynet_core::l_gen_session_id(lua_State* L)
{
    service_context* svc_ctx = (service_context*)lua_touserdata(L, lua_upvalueindex(1));

    // gen session id
    int session_id = service_manager::instance()->send(svc_ctx, 0, 0, MESSAGE_TAG_ALLOC_SESSION, 0, nullptr, 0);
    lua_pushinteger(L, session_id);

    return 1;
}

int skynet_core::l_log(lua_State* L)
{
    //
    service_context* svc_ctx = (service_context*)lua_touserdata(L, lua_upvalueindex(1));

    int n = lua_gettop(L);
    if (n <= 1)
    {
        lua_settop(L, 1);
        const char* s = luaL_tolstring(L, 1, nullptr);
        log(svc_ctx, "%s", s);
    }
    else
    {
        //
        luaL_Buffer b;
        luaL_buffinit(L, &b);
        for (int i = 1; i <= n; i++)
        {
            // 将给定索引处的 Lua 值转换为一个相应格式的 C 字符串。 结果串不仅会压栈，还会由函数返回。
            luaL_tolstring(L, i, nullptr);
            // 把栈顶的值添加到缓冲器B, 弹出该值。
            luaL_addvalue(&b);
            if (i < n)
            {
                luaL_addchar(&b, ' ');
            }
        }
        // 结束对缓冲器B的使用，把最终字符串留在栈顶。
        luaL_pushresult(&b);
        log(svc_ctx, "%s", lua_tostring(L, -1));
    }

    return 0;
}

/*
    string tag
    string userstring
    thread co (default nil/current L)
    integer level (default nil)
 */
int skynet_core::l_trace(lua_State* L)
{
    service_context* svc_ctx = (service_context*)lua_touserdata(L, lua_upvalueindex(1));
    const char* tag = luaL_checkstring(L, 1);
    const char* user = luaL_checkstring(L, 2);
    if (!lua_isnoneornil(L, 3))
    {
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
        struct source_info si[MAX_LEVEL];
        lua_Debug d;
        int index = 0;
        do
        {
            if (!lua_getstack(co, level, &d))
                break;
            lua_getinfo(co, "Sl", &d);
            level++;
            si[index].source = d.source;
            si[index].line = d.currentline;
            if (d.currentline >= 0)
                ++index;
        } while (index < MAX_LEVEL);
        switch (index)
        {
        case 1:
            log(svc_ctx, "<TRACE %s> %" PRId64 " %s : %s:%d", tag, time_helper::get_time_ns(), user, si[0].source, si[0].line);
            break;
        case 2:
            log(svc_ctx, "<TRACE %s> %" PRId64 " %s : %s:%d %s:%d", tag, time_helper::get_time_ns(), user,
            si[0].source, si[0].line, si[1].source, si[1].line);
            break;
        case 3:
            log(svc_ctx, "<TRACE %s> %" PRId64 " %s : %s:%d %s:%d %s:%d", tag, time_helper::get_time_ns(), user,
            si[0].source, si[0].line, si[1].source, si[1].line, si[2].source, si[2].line);
            break;
        default:
            log(svc_ctx, "<TRACE %s> %" PRId64 " %s", tag, time_helper::get_time_ns(), user);
            break;
        }
        return 0;
    }

    log(svc_ctx, "<TRACE %s> %" PRId64 " %s", tag, time_helper::get_time_ns(), user);

    return 0;
}

//----------------------------------------------
// without service_context
//----------------------------------------------


int skynet_core::l_to_string(lua_State* L)
{
    // is invalid or nil, just return
    if (lua_isnoneornil(L, 1) != 0)
        return 0;

    char* msg = (char*)lua_touserdata(L, 1);
    int msg_sz = luaL_checkinteger(L, 2);
    lua_pushlstring(L, msg, msg_sz);

    return 1;
}

int skynet_core::l_pack(lua_State* L)
{
    return luaseri_pack(L);
}

int skynet_core::l_unpack(lua_State* L)
{
    return luaseri_unpack(L);
}

int skynet_core::l_pack_string(lua_State* L)
{
    luaseri_pack(L);
    char* str = (char*) lua_touserdata(L, -2);
    int sz = lua_tointeger(L, -1);
    lua_pushlstring(L, str, sz);
    // skynet_free(str);
    delete[] str;

    return 1;
}

// 释放消息
int skynet_core::l_trash(lua_State* L)
{
    int t = lua_type(L, 1);
    switch (t)
    {
    case LUA_TSTRING:
        break;
    case LUA_TLIGHTUSERDATA:
    {
        void* msg = lua_touserdata(L, 1);
        luaL_checkinteger(L, 2);
        // skynet_free(msg);
        delete[] msg;
    }
        break;
    default:
        luaL_error(L, "skynet.trash invalid param %s", lua_typename(L, t));
        break;
    }

    return 0;
}

int skynet_core::l_now(lua_State* L)
{
    uint64_t ti = timer_manager::instance()->now();
    lua_pushinteger(L, ti);
    return 1;
}

int skynet_core::l_hpc(lua_State* L)
{
    lua_pushinteger(L, time_helper::get_time_ns());
    return 1;
}

} }

/**
 * skynet luaclib - skynet.core
 */
LUAMOD_API int luaopen_skynet_core(lua_State* L)
{
    luaL_checkversion(L);

    // service_context functions
    luaL_Reg l[] = {
        { "send",           skynet::luaclib::skynet_core::l_send },
        { "redirect",       skynet::luaclib::skynet_core::l_redirect },
        { "command",        skynet::luaclib::skynet_core::l_service_command },
        { "intcommand",     skynet::luaclib::skynet_core::l_service_command_int },
        { "addresscommand", skynet::luaclib::skynet_core::l_service_command_address },
        { "callback",       skynet::luaclib::skynet_core::l_set_service_callback },
        { "gen_session_id", skynet::luaclib::skynet_core::l_gen_session_id },
        { "log",            skynet::luaclib::skynet_core::l_log },
        { "trace",          skynet::luaclib::skynet_core::l_trace },

        { nullptr, nullptr },
    };
    int l_sz = sizeof(l) / sizeof(l[0]) - 1;

    // functions without service_context
    luaL_Reg l2[] = {
        { "tostring",    skynet::luaclib::skynet_core::l_to_string },
        { "pack",        skynet::luaclib::skynet_core::l_pack },
        { "unpack",      skynet::luaclib::skynet_core::l_unpack },
        { "pack_string", skynet::luaclib::skynet_core::l_pack_string },
        { "trash",       skynet::luaclib::skynet_core::l_trash },
        { "now",         skynet::luaclib::skynet_core::l_now },
        { "hpc",         skynet::luaclib::skynet_core::l_hpc },

        { nullptr, nullptr },
    };
    int l2_sz = sizeof(l2) / sizeof(l2[0]) - 1;

    lua_createtable(L, 0, l_sz + l2_sz);

    // get service_context from global register (see: service_snlua::init_cb()), push it to stack top.
    // can use it by lua_upvalueindex(1)
    lua_getfield(L, LUA_REGISTRYINDEX, "service_context");
    skynet::service_context* svc_ctx = (skynet::service_context*)lua_touserdata(L, -1);
    if (svc_ctx == nullptr)
    {
        return luaL_error(L, "Init skynet service context first");
    }

    // shared service_context
    luaL_setfuncs(L, l, 1);

    // without service_context
    luaL_setfuncs(L, l2, 0);

    return 1;
}
