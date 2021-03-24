#include "snlua_service.h"
#include "skynet.h"

extern "C" {
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
}

#include <cassert>

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

static const char* _get_env(service_context* ctx, const char* key, const char* default_value)
{
    const char* ret = service_command::exec(ctx, "GETENV", key);
    if (ret == nullptr)
    {
        return default_value;
    }

    return ret;
}

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
    service_manager::instance()->send_by_name(ctx, 0, ".launcher",
        message_protocol_type::MSG_PTYPE_TEXT, 0, (void*)"ERROR", 5);
}

snlua_service::snlua_service()
{
    mem_report_ = MEMORY_WARNING_REPORT;
    mem_limit_ = 0;
//    L_ = lua_newstate(snlua_service::lalloc, this);
    L_ = luaL_newstate();
}

snlua_service::~snlua_service()
{
    fini();
}

bool snlua_service::init(service_context* svc_ctx, const char* param)
{
    // copy param
    int param_sz = ::strlen(param);
    char* tmp_param = new char[param_sz];
    ::memcpy(tmp_param, param, param_sz);

    //
    svc_ctx->set_callback(snlua_cb, this);

    // query self service handle
    std::string self_svc_handle_string = service_command::exec(svc_ctx, "REG");
    uint32_t self_svc_handle = std::stoi(self_svc_handle_string.substr(1), nullptr, 16);

    // send param to self, it must be first message
    service_manager::instance()->send(svc_ctx, 0, self_svc_handle, MESSAGE_TAG_DONT_COPY, 0, tmp_param, param_sz);

    return true;
}

void snlua_service::fini()
{
    if (L_ != nullptr)
    {
        lua_close(L_);
        L_ = nullptr;
    }
}

void snlua_service::signal(int signal)
{
    log(svc_ctx_, "recv a signal %d", signal);

    if (signal == 0)
    {
#ifdef lua_checksig
        // If our lua support signal (modified lua version by skynet), trigger it.
        skynet_sig_L = L_;
#endif
    }
    else if (signal == 1)
    {
        log(svc_ctx_, "Current Memory %.3fK", (float)mem_ / 1024);
    }
}

int snlua_service::snlua_cb(service_context* svc_ctx, void* ud, int msg_ptype, int session, uint32_t src_svc_handle, const void* msg, size_t sz)
{
    assert(msg_ptype == 0 && session == 0);

    auto svc_ptr = (snlua_service*)ud;

    // reset service message callback
    svc_ctx->set_callback(nullptr, nullptr);

    // 设置各项资源路径参数, 并加载 loader.lua
    // 在init_cb里进行Lua层的初始化，比如初始化LUA_PATH，LUA_CPATH，LUA_SERVICE等全局变量
    bool ret = init_lua_cb(svc_ptr, svc_ctx, (const char*)msg, sz);
    if (!ret)
    {
        service_command::exec(svc_ctx, "EXIT");
    }

    return 0;
}

bool snlua_service::init_lua_cb(snlua_service* svc_ptr, service_context* svc_ctx, const char* args, size_t sz)
{
    lua_State* L = svc_ptr->L_;
    svc_ptr->svc_ctx_ = svc_ctx;

    //
    lua_gc(L, LUA_GCSTOP, 0);

    // signal for libraries to ignore env. vars.
    lua_pushboolean(L, 1);
    lua_setfield(L, LUA_REGISTRYINDEX, "LUA_NOENV");

    //
    luaL_openlibs(L);

    // set service context to global register, it's upvalue
    lua_pushlightuserdata(L, svc_ctx);
    lua_setfield(L, LUA_REGISTRYINDEX, "service_context");
    luaL_requiref(L, "skynet.codecache", codecache, 0);
    lua_pop(L, 1);

    // lualib path
    const char* path_lualib = _get_env(svc_ctx, "lua_path", "./lualib/?.lua;./lualib/?/init.lua");
    lua_pushstring(L, path_lualib);
    lua_setglobal(L, "LUA_PATH");

    // luaclib path
    const char* path_luaclib = _get_env(svc_ctx, "lua_cpath", "./luaclib/?.so");
    lua_pushstring(L, path_luaclib);
    lua_setglobal(L, "LUA_CPATH");

    // lua service path
    const char* path_service_lua = _get_env(svc_ctx, "luaservice", "./service/?.lua");
    lua_pushstring(L, path_service_lua);
    lua_setglobal(L, "LUA_SERVICE");

    // preload, before lua service
    const char* preload = service_command::exec(svc_ctx, "GETENV", "preload");
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
        return false;
    }

    // use loader.lua to load bootstrap lua service
    lua_pushlstring(L, args, sz);
    r = lua_pcall(L, 1, 0, 1);
    if (r != LUA_OK)
    {
        log(svc_ctx, "lua loader error : %s", lua_tostring(L, -1));
        report_launcher_error(svc_ctx);
        return false;
    }

    //
    lua_settop(L, 0);

    // vm memory limit
    if (lua_getfield(L, LUA_REGISTRYINDEX, "memlimit") == LUA_TNUMBER)
    {
        size_t limit = lua_tointeger(L, -1);
        svc_ptr->mem_limit_ = limit;
        log(svc_ctx, "Set memory limit to %.2f M", (float)limit / (1024 * 1024));
        lua_pushnil(L);
        lua_setfield(L, LUA_REGISTRYINDEX, "memlimit");
    }
    lua_pop(L, 1);

    //
    lua_gc(L, LUA_GCRESTART, 0);

    return true;
}

/**
 * lua memory allocator
 */
void* snlua_service::lalloc(void* ud, void* ptr, size_t osize, size_t nsize)
{
    auto svc_ptr = (snlua_service*)ud;

    // ajuest used memory
    size_t mem = svc_ptr->mem_;
    svc_ptr->mem_ += nsize;
    if (ptr != nullptr)
        svc_ptr->mem_ -= osize;

    // check memory limit
    if (svc_ptr->mem_limit_ != 0 && svc_ptr->mem_ > svc_ptr->mem_limit_)
    {
        if (ptr == nullptr || nsize > osize)
        {
            svc_ptr->mem_ = mem;
            return nullptr;
        }
    }

    // 检查内存分配报告阈值
    if (svc_ptr->mem_ > svc_ptr->mem_report_)
    {
        svc_ptr->mem_report_ *= 2;
        log(svc_ptr->svc_ctx_, "Memory warning %.2f M", (float)svc_ptr->mem_ / (1024 * 1024));
    }

    return skynet_lalloc(ptr, osize, nsize);
}

} }

