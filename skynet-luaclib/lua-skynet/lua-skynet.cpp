/**
 * skynet luaclib - skynet.core
 */

#define LUA_LIB

#include "skynet.h"

#include "lua-seri.h"

#include "node/skynet_command.h"
#include "log/log.h"
#include "mq/mq_msg.h"
#include "service/service_context.h"
#include "timer/timer_manager.h"

extern "C" {
#include <lua.h>
#include <lauxlib.h>
}

#include <string>
#include <cstdlib>
#include <cassert>
#include <ctime>
#include <cinttypes>

#if defined(__APPLE__)
#include <sys/time.h>
#endif

namespace skynet { namespace luaclib {

// 
#define KNRM  "\x1B[0m"
#define KRED  "\x1B[31m"


// return nsec
static int64_t get_time()
{
#if !defined(__APPLE__) || defined(AVAILABLE_MAC_OS_X_VERSION_10_12_AND_LATER)
    struct timespec ti;
    ::clock_gettime(CLOCK_MONOTONIC, &ti);
    return (int64_t) 1000000000 * ti.tv_sec + ti.tv_nsec;
#else
    struct timeval tv;
    ::gettimeofday(&tv, NULL);
    return (int64_t)1000000000 * tv.tv_sec + tv.tv_usec * 1000;
#endif
}

struct snlua
{
    lua_State* L;
    service_context* ctx;
    const char* preload;
};

static int traceback(lua_State* L)
{
    const char* msg = lua_tostring(L, 1);
    if (msg)
        luaL_traceback(L, L, msg, 1);
    else
    {
        lua_pushliteral(L, "(no error message)");
    }
    return 1;
}

// 在_cb里，最终会调用Lua层的dispatch_message，参数依次是：type, msg, sz, session, source。
// 所以，snlua类型的服务收到消息时最终会调用Lua层的消息回调函数skynet.dispatch_message。
static int _cb(service_context* context, void* ud, int type, int session, uint32_t source, const void* msg, size_t sz)
{
    lua_State* L = (lua_State*)ud;
    int trace = 1;
    int r;
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
    lua_pushlightuserdata(L, (void*) msg);
    lua_pushinteger(L, sz);
    lua_pushinteger(L, session);
    lua_pushinteger(L, source);

    r = lua_pcall(L, 5, 0, trace);
    if (r == LUA_OK)
    {
        return 0;
    }
    
    const char* self = skynet_command::handle_command(context, "REG", NULL);
    switch (r)
    {
        case LUA_ERRRUN:
            log(context, "lua call [%x to %s : %d msgsz = %d] error : " KRED "%s" KNRM, source, self, session, sz, lua_tostring(L, -1));
            break;
        case LUA_ERRMEM:
            log(context, "lua memory error : [%x to %s : %d]", source, self, session);
            break;
        case LUA_ERRERR:
            log(context, "lua error in error : [%x to %s : %d]", source, self, session);
            break;
        case LUA_ERRGCMM:
            log(context, "lua gc error : [%x to %s : %d]", source, self, session);
            break;
    };

    lua_pop(L, 1);

    return 0;
}

static int forward_cb(service_context* context, void* ud, int type, int session, uint32_t source, const void* msg, size_t sz)
{
    _cb(context, ud, type, session, source, msg, sz);
    // don't delete msg in forward mode.
    return 1;
}

// 通过lua_upvalueindex获取函数的上值ctx，然后设置服务的消息回调函数为_cb，此时Lua堆栈上有且仅有一个元素lua函数(skynet.dispatch_message)
// 在_cb里，最终会调用Lua层的dispatch_message，参数依次是：type, msg, sz, session, source。
// 所以，snlua类型的服务收到消息时最终会调用Lua层的消息回调函数skynet.dispatch_message。
static int l_callback(lua_State* L)
{
    service_context* context = (service_context*)lua_touserdata(L, lua_upvalueindex(1));
    int forward = lua_toboolean(L, 2);
    luaL_checktype(L, 1, LUA_TFUNCTION);
    lua_settop(L, 1);
    lua_rawsetp(L, LUA_REGISTRYINDEX, (void*)_cb);

    lua_rawgeti(L, LUA_REGISTRYINDEX, LUA_RIDX_MAINTHREAD);
    lua_State* gL = lua_tothread(L, -1);

    if (forward)
    {
        context->callback(gL, forward_cb);
    }
    else
    {
        context->callback(gL, _cb);
    }

    return 0;
}

static int l_command(lua_State* L)
{
    service_context* context = (service_context*)lua_touserdata(L, lua_upvalueindex(1));
    const char* cmd = luaL_checkstring(L, 1);
    const char* result;
    const char* parm = NULL;
    if (lua_gettop(L) == 2)
    {
        parm = luaL_checkstring(L, 2);
    }

    result = skynet_command::handle_command(context, cmd, parm);
    if (result)
    {
        lua_pushstring(L, result);
        return 1;
    }
    return 0;
}

static int l_address_command(lua_State* L)
{
    service_context* context = (service_context*)lua_touserdata(L, lua_upvalueindex(1));
    const char* cmd = luaL_checkstring(L, 1);
    const char* result;
    const char* parm = NULL;
    if (lua_gettop(L) == 2)
    {
        parm = luaL_checkstring(L, 2);
    }
    result = skynet_command::handle_command(context, cmd, parm);
    if (result && result[0] == ':')
    {
        int i;
        uint32_t addr = 0;
        for (i = 1; result[i]; i++)
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

// 
static int l_int_command(lua_State* L)
{
    service_context* context = (service_context*)lua_touserdata(L, lua_upvalueindex(1));
    const char* cmd = luaL_checkstring(L, 1);
    const char* result;
    const char* parm = NULL;
    char tmp[64];    // for integer parm
    if (lua_gettop(L) == 2)
    {
        if (lua_isnumber(L, 2))
        {
            int32_t n = (int32_t) luaL_checkinteger(L, 2);
            sprintf(tmp, "%d", n);
            parm = tmp;
        }
        else
        {
            parm = luaL_checkstring(L, 2);
        }
    }

    result = skynet_command::handle_command(context, cmd, parm);
    if (result)
    {
        char* endptr = NULL;
        lua_Integer r = strtoll(result, &endptr, 0);
        if (endptr == NULL || *endptr != '\0')
        {
            // may be real number
            double n = strtod(result, &endptr);
            if (endptr == NULL || *endptr != '\0')
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

// 
static int l_genid(lua_State* L)
{
    service_context* context = (service_context*)lua_touserdata(L, lua_upvalueindex(1));
    int session = skynet_send(context, 0, 0, message_type::TAG_ALLOC_SESSION, 0, NULL, 0);
    lua_pushinteger(L, session);
    return 1;
}

// 
static const char* get_dest_string(lua_State* L, int index)
{
    const char* dest_string = lua_tostring(L, index);
    if (dest_string == NULL)
    {
        luaL_error(L, "dest address type (%s) must be a string or number.", lua_typename(L, lua_type(L, index)));
    }
    return dest_string;
}

// 
static int send_message(lua_State* L, int source, int idx_type)
{
    service_context* context = (service_context*)lua_touserdata(L, lua_upvalueindex(1));
    uint32_t dest = (uint32_t) lua_tointeger(L, 1);
    const char* dest_string = NULL;
    if (dest == 0)
    {
        if (lua_type(L, 1) == LUA_TNUMBER)
        {
            return luaL_error(L, "Invalid service address 0");
        }
        dest_string = get_dest_string(L, 1);
    }

    int type = luaL_checkinteger(L, idx_type + 0);
    int session = 0;
    if (lua_isnil(L, idx_type + 1))
    {
        type |= message_type::TAG_ALLOC_SESSION;
    }
    else
    {
        session = luaL_checkinteger(L, idx_type + 1);
    }

    int mtype = lua_type(L, idx_type + 2);
    switch (mtype)
    {
    case LUA_TSTRING:
    {
        size_t len = 0;
        void* msg = (void*) lua_tolstring(L, idx_type + 2, &len);
        if (len == 0)
        {
            msg = NULL;
        }
        if (dest_string)
        {
            session = skynet_send_by_name(context, source, dest_string, type, session, msg, len);
        }
        else
        {
            session = skynet_send(context, source, dest, type, session, msg, len);
        }
        break;
    }
    case LUA_TLIGHTUSERDATA:
    {
        void* msg = lua_touserdata(L, idx_type + 2);
        int size = luaL_checkinteger(L, idx_type + 3);
        if (dest_string)
        {
            session = skynet_send_by_name(context, source, dest_string, type | message_type::TAG_DONT_COPY, session, msg, size);
        }
        else
        {
            session = skynet_send(context, source, dest, type | message_type::TAG_DONT_COPY, session, msg, size);
        }
        break;
    }
    default:
        luaL_error(L, "invalid param %s", lua_typename(L, lua_type(L, idx_type + 2)));
    }
    if (session < 0)
    {
        if (session == -2)
        {
            // package is too large
            lua_pushboolean(L, 0);
            return 1;
        }
        // send to invalid address
        // todo: maybe throw an error would be better
        return 0;
    }
    lua_pushinteger(L, session);
    return 1;
}

/**
 *  uint32 address
 *   string address
 *  integer type
 *  integer session
 *  string message
 *   lightuserdata message_ptr
 *   integer len
 */
static int l_send(lua_State* L)
{
    return send_message(L, 0, 2);
}


/**
 *  uint32 address
 *   string address
 *  integer source_address
 *  integer type
 *  integer session
 *  string message
 *   lightuserdata message_ptr
 *   integer len
 */
static int l_redirect(lua_State* L)
{
    uint32_t source = (uint32_t) luaL_checkinteger(L, 2);        // 检测并返回source整形地址值, 错误会抛出异常
    return send_message(L, source, 3);
}

static int l_error(lua_State* L)
{
    service_context* context = (service_context*)lua_touserdata(L, lua_upvalueindex(1));
    // 参数个数
    int n = lua_gettop(L);
    if (n <= 1)
    {
        lua_settop(L, 1);
        const char* s = luaL_tolstring(L, 1, NULL);
        log(context, "%s", s);
        return 0;
    }

    //
    luaL_Buffer b;
    luaL_buffinit(L, &b);
    int i;
    for (i = 1; i <= n; i++)
    {
        // 将给定索引处的 Lua 值转换为一个相应格式的 C 字符串。 结果串不仅会压栈，还会由函数返回。
        luaL_tolstring(L, i, NULL);
        // 把栈顶的值添加到缓冲器B, 弹出该值。
        luaL_addvalue(&b);
        if (i < n)
        {
            luaL_addchar(&b, ' ');
        }
    }
    // 结束对缓冲器B的使用，把最终字符串留在栈顶。
    luaL_pushresult(&b);
    log(context, "%s", lua_tostring(L, -1));
    return 0;
}

static int l_to_string(lua_State* L)
{
    if (lua_isnoneornil(L, 1))
    {
        return 0;
    }
    char* msg = (char*)lua_touserdata(L, 1);
    int sz = luaL_checkinteger(L, 2);
    lua_pushlstring(L, msg, sz);
    return 1;
}

static int l_pack_string(lua_State* L)
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
static int l_trash(lua_State* L)
{
    int t = lua_type(L, 1);
    switch (t)
    {
        case LUA_TSTRING:
        {
            break;
        }
        case LUA_TLIGHTUSERDATA:
        {
            void* msg = lua_touserdata(L, 1);
            luaL_checkinteger(L, 2);
            // skynet_free(msg);
            break;
        }
        default:
            luaL_error(L, "skynet.trash invalid param %s", lua_typename(L, t));
    }

    return 0;
}

static int l_now(lua_State* L)
{
    uint64_t ti = timer_manager::instance()->now();
    lua_pushinteger(L, ti);
    return 1;
}

static int lhpc(lua_State* L)
{
    lua_pushinteger(L, get_time());
    return 1;
}

#define MAX_LEVEL 3

struct source_info
{
    const char* source;
    int line;
};

/*
    string tag
    string userstring
    thread co (default nil/current L)
    integer level (default nil)
 */
static int l_trace(lua_State* L)
{
    service_context* context = (service_context*)lua_touserdata(L, lua_upvalueindex(1));
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
            log(context, "<TRACE %s> %" PRId64 " %s : %s:%d", tag, get_time(), user, si[0].source, si[0].line);
            break;
        case 2:
            log(context, "<TRACE %s> %" PRId64 " %s : %s:%d %s:%d", tag, get_time(), user,
                        si[0].source, si[0].line, si[1].source, si[1].line);
            break;
        case 3:
            log(context, "<TRACE %s> %" PRId64 " %s : %s:%d %s:%d %s:%d", tag, get_time(), user,
                        si[0].source, si[0].line, si[1].source, si[1].line, si[2].source, si[2].line);
            break;
        default:
            log(context, "<TRACE %s> %" PRId64 " %s", tag, get_time(), user);
            break;
        }
        return 0;
    }
    log(context, "<TRACE %s> %" PRId64 " %s", tag, get_time(), user);
    return 0;
}

} }

// skynet.core
LUAMOD_API int luaopen_skynet_core(lua_State* L)
{
    luaL_checkversion(L);

    // 提供了很多注册函数供Lua层调用
    luaL_Reg l[] = {
        { "send", skynet::luaclib::l_send },
        { "genid", skynet::luaclib::l_genid },
        { "redirect", skynet::luaclib::l_redirect },
        { "command", skynet::luaclib::l_command },
        { "intcommand", skynet::luaclib::l_int_command },
        { "addresscommand", skynet::luaclib::l_address_command },
        { "error", skynet::luaclib::l_error },
        { "callback", skynet::luaclib::l_callback },
        { "trace", skynet::luaclib::l_trace },

        { nullptr, nullptr },
    };

    // functions without service_context
    luaL_Reg l2[] = {
        { "tostring", skynet::luaclib::l_to_string },
        { "pack", luaseri_pack },
        { "unpack", luaseri_unpack },
        { "packstring", skynet::luaclib::l_pack_string },
        { "trash", skynet::luaclib::l_trash },
        { "now", skynet::luaclib::l_now },
        { "hpc", skynet::luaclib::lhpc },    // getHPCounter

        { nullptr,         nullptr },
    };

    lua_createtable(L, 0, sizeof(l) / sizeof(l[0]) + sizeof(l2) / sizeof(l2[0]) - 2);

    // 从LUA_REGISTERINDEX表中获取ctx(在init_cb里设置的)，这些注册函数共用ctx这个上值，在C api里通过lua_upvalueindex(1)获取这个ctx，然后对ctx进行相应处理。
    lua_getfield(L, LUA_REGISTRYINDEX, "service_context");
    skynet::service_context* ctx = (skynet::service_context*)lua_touserdata(L, -1);
    if (ctx == nullptr)
    {
        return luaL_error(L, "Init skynet context first");
    }

    luaL_setfuncs(L, l, 1);
    luaL_setfuncs(L, l2, 0);

    return 1;
}
