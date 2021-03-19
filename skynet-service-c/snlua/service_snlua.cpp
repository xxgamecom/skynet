/**
 * skynet lua service sandbox
 * 
 * skynet绝大部分服务类型是snlua，它是运行Lua脚本的服务，
 * 在用skynet框架上开发游戏服务器时，大部分逻辑都是snlua服务，90%以上只需写Lua代码即可
 */

#include "skynet.h"
#include "snlua_mod.h"
#include "profile.h"

#include <cassert>
#include <cstdlib>
#include <ctime>

extern "C" {
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
}

namespace skynet { namespace service {


/**
 * 是否在虚拟机之间共享函数原型
 *
 * 参考lualib.h中定义:
 * LUAMOD_API int (luaopen_cache) (lua_State *L);
 * LUALIB_API void (luaL_initcodecache) (void);
 */
// LUA_CACHELIB may defined in patched lua for shared proto
#ifdef LUA_CACHELIB

#define codecache luaopen_cache

#else

static int _dummy(lua_State* L)
{
    return 0;
}

static int codecache(lua_State* L)
{
    luaL_Reg l[] = {
        { "clear", _dummy },
        { "mode", _dummy },

        { nullptr, nullptr },
    };
    luaL_newlib(L, l);

    lua_getglobal(L, "loadfile");
    lua_setfield(L, -2, "loadfile");

    return 1;
}

#endif


/**
 * track trace
 */
static int traceback(lua_State* L)
{
    const char* msg = lua_tostring(L, 1);
    if (msg != nullptr)
    {
        // after call, use lua_tostring(L,-1) can get full stack trace info
        luaL_traceback(L, L, msg, 1);
    }
    else
    {
        lua_pushliteral(L, "(no error message)");
    }

    return 1;
}

static void report_launcher_error(service_context* ctx)
{
    // sizeof "ERROR" == 5
    service_manager::instance()->send_by_name(ctx, 0, ".launcher", message_protocol_type::PTYPE_TEXT, 0, (void*)"ERROR", 5);
}

static const char* _get_env(service_context* ctx, const char* key, const char* default_value)
{
    const char* ret = service_command::handle_command(ctx, "GETENV", key);
    if (ret == nullptr)
    {
        return default_value;
    }

    return ret;
}

/**
 * init lua service message callback
 *
 * 设置一些虚拟机环境变量 (主要是路径资源之类的)
 * 在init_cb里进行Lua层的初始化，比如初始化LUA_PATH，LUA_CPATH，LUA_SERVICE等全局变量
 */
static int init_lua_cb(snlua_mod* mod_ptr, service_context* svc_ctx, const char* args, size_t sz)
{
    lua_State* L = mod_ptr->L;
    mod_ptr->svc_ctx = svc_ctx;

    //
    lua_gc(L, LUA_GCSTOP, 0);

    // 注册表["LUA_NOENV"] = true, 通知库忽略环境变量
    lua_pushboolean(L, 1);  /* signal for libraries to ignore env. vars. */
    lua_setfield(L, LUA_REGISTRYINDEX, "LUA_NOENV");

    //
    luaL_openlibs(L);

    // skynet.profile
    luaL_requiref(L, "skynet.profile", init_profile, 0);
    int profile_lib = lua_gettop(L);

    // replace coroutine.resume / coroutine.wrap
    lua_getglobal(L, "coroutine");
    lua_getfield(L, profile_lib, "resume");
    lua_setfield(L, -2, "resume");
    lua_getfield(L, profile_lib, "wrap");
    lua_setfield(L, -2, "wrap");

    lua_settop(L, profile_lib - 1);

    // set service context to global register, it's upvalue
    lua_pushlightuserdata(L, svc_ctx);
    lua_setfield(L, LUA_REGISTRYINDEX, "service_context");

    // code cache
    luaL_requiref(L, "skynet.codecache", codecache, 0);
    lua_pop(L, 1);

    lua_gc(L, LUA_GCGEN, 0, 0);

    // lualib path
    const char* path = _get_env(svc_ctx, "lua_path", "./lualib/?.lua;./lualib/?/init.lua");
    lua_pushstring(L, path);
    lua_setglobal(L, "LUA_PATH");

    // luaclib path
    const char* cpath = _get_env(svc_ctx, "lua_cpath", "./luaclib/?.so");
    lua_pushstring(L, cpath);
    lua_setglobal(L, "LUA_CPATH");

    // lua service path
    const char* service = _get_env(svc_ctx, "luaservice", "./service/?.lua");
    lua_pushstring(L, service);
    lua_setglobal(L, "LUA_SERVICE");

    // preload, lua服务运行前执行, 设置全局变量LUA_PRELOAD
    const char* preload = service_command::handle_command(svc_ctx, "GETENV", "preload");
    lua_pushstring(L, preload);
    lua_setglobal(L, "LUA_PRELOAD");

    // set stack trace
    lua_pushcfunction(L, traceback);
    assert(lua_gettop(L) == 1);

    // load loader.lua
    const char* loader = _get_env(svc_ctx, "lualoader", "./lualib/loader.lua");
    int r = luaL_loadfile(L, loader);
    if (r != LUA_OK)
    {
        log(svc_ctx, "Can't load %s : %s", loader, lua_tostring(L, -1));
        report_launcher_error(svc_ctx);
        return 1;
    }

    // run loader.lua，args “bootstrap” (load bootstrap lua service)
    lua_pushlstring(L, args, sz);
    r = lua_pcall(L, 1, 0, 1);
    if (r != LUA_OK)
    {
        service_command::handle_command(svc_ctx, "lua loader error : %s", lua_tostring(L, -1));
        report_launcher_error(svc_ctx);
        return 1;
    }

    //
    lua_settop(L, 0);

    // vm memory limit
    if (lua_getfield(L, LUA_REGISTRYINDEX, "memlimit") == LUA_TNUMBER)
    {
        size_t limit = lua_tointeger(L, -1);
        mod_ptr->mem_limit = limit;
        log(svc_ctx, "Set memory limit to %.2f M", (float)limit / (1024 * 1024));
        lua_pushnil(L);
        lua_setfield(L, LUA_REGISTRYINDEX, "memlimit");
    }
    lua_pop(L, 1);

    //
    lua_gc(L, LUA_GCRESTART, 0);

    return 0;
}

// snlua service message callback
static int snlua_cb(service_context* svc_ctx, void* ud, int msg_ptype, int session, uint32_t src_svc_handle, const void* msg, size_t sz)
{
    assert(msg_ptype == 0 && session == 0);

    auto mod_ptr = (snlua_mod*)ud;

    // reset service message callback
    svc_ctx->set_callback(nullptr, nullptr);

    // 设置各项资源路径参数, 并加载 loader.lua
    // 在init_cb里进行Lua层的初始化，比如初始化LUA_PATH，LUA_CPATH，LUA_SERVICE等全局变量
    int err = init_lua_cb(mod_ptr, svc_ctx, (const char*)msg, sz);
    if (err != 0)
    {
        service_command::handle_command(svc_ctx, "EXIT");
    }

    return 0;
}

/**
 * lua memory allocator
 */
static void* lalloc(void* ud, void* ptr, size_t osize, size_t nsize)
{
    auto mod_ptr = (snlua_mod*)ud;

    // ajuest used memory
    size_t mem = mod_ptr->mem;
    mod_ptr->mem += nsize;
    if (ptr != nullptr)
        mod_ptr->mem -= osize;

    // check memory limit
    if (mod_ptr->mem_limit != 0 && mod_ptr->mem > mod_ptr->mem_limit)
    {
        if (ptr == nullptr || nsize > osize)
        {
            mod_ptr->mem = mem;
            return nullptr;
        }
    }

    // 检查内存分配报告阈值
    if (mod_ptr->mem > mod_ptr->mem_report)
    {
        mod_ptr->mem_report *= 2;
        log(mod_ptr->svc_ctx, "Memory warning %.2f M", (float)mod_ptr->mem / (1024 * 1024));
    }

    return skynet_lalloc(ptr, osize, nsize);
}

//-------------------------------------------
// mod interface
//-------------------------------------------

snlua_mod* snlua_create()
{
    auto mod_ptr = new snlua_mod;

    mod_ptr->mem_report = MEMORY_WARNING_REPORT;
    mod_ptr->mem_limit = 0;
    mod_ptr->L = lua_newstate(lalloc, mod_ptr);
    mod_ptr->activeL = nullptr;
    mod_ptr->trap = 0;

    return mod_ptr;
}

void snlua_release(snlua_mod* mod_ptr)
{
    if (mod_ptr->L != nullptr)
        lua_close(mod_ptr->L);

    delete mod_ptr;
}

int snlua_init(snlua_mod* mod_ptr, service_context* ctx, const char* param)
{
    //
    ctx->set_callback(mod_ptr, snlua_cb);

    int sz = ::strlen(param);
    char* tmp = (char*)skynet_malloc(sz);
    ::memcpy(tmp, param, sz);

    // send param to self, it must be first message
    const char* self = service_command::handle_command(ctx, "REG");
    uint32_t self_svc_handle = ::strtoul(self + 1, nullptr, 16);
    service_manager::instance()->send(ctx, 0, self_svc_handle, MESSAGE_TAG_DONT_COPY, 0, tmp, sz);

    return 0;
}

void snlua_signal(snlua_mod* mod_ptr, int signal)
{
    log(mod_ptr->svc_ctx, "recv a signal %d", signal);

    if (signal == 0)
    {
        if (mod_ptr->trap == 0)
        {
            // only one thread can set trap ( mod_ptr->trap 0->1 )
            int zero = 0;
            if (!mod_ptr->trap.compare_exchange_strong(zero, 1))
                return;

            lua_sethook(mod_ptr->activeL, signal_hook, LUA_MASKCOUNT, 1);

            // finish set ( mod_ptr->trap 1 -> -1 )
            int one = 1;
            mod_ptr->trap.compare_exchange_strong(one, -1);
        }
    }
    else if (signal == 1)
    {
        log(mod_ptr->svc_ctx, "Current Memory %.3fK", (float)mod_ptr->mem / 1024);
    }
}

} }
