/**
 * skynet lua服务沙盒
 * 
 * skynet绝大部分服务类型是snlua，它是运行Lua脚本的服务，在用skynet框架上开发游戏服务器时，大部分逻辑都是snlua服务，90%以上只需写Lua代码即可
 */

#include "skynet.h"

#include <cassert>
#include <string>
#include <cstdlib>
// #include <stdio.h>

extern "C" {
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
}

namespace skynet { namespace service {

#define MEMORY_WARNING_REPORT (1024 * 1024 * 32)

// snlua c service own data
struct snlua
{
    lua_State*                  L = nullptr;
    service_context*            svc_ctx = nullptr;
    size_t                      mem = 0;                        // 已占用内存
    size_t                      mem_report = 0;                 // 内存报告阈值
    size_t                      mem_limit = 0;                  // 最大内存分配限制值
};

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

static int cleardummy(lua_State* L)
{
    return 0;
}

static int codecache(lua_State* L)
{
    luaL_Reg l[] = {
        { "clear", cleardummy },
        { "mode", cleardummy },

        { nullptr, nullptr },
    };
    luaL_newlib(L, l);
    lua_getglobal(L, "loadfile");
    lua_setfield(L, -2, "loadfile");

    return 1;
}

#endif

// 堆栈跟踪
static int traceback(lua_State* L)
{
    const char* msg = lua_tostring(L, 1);
    if (msg)
    {
        // 打印发生错误时的堆栈信息, msg是错误信息, 附加到打印的堆栈最前端, 1是指从第一层开始回溯
        // 这个函数之后,lua_tostring(L,-1)即可获取完整的堆栈和错误信息
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
    service_manager::instance()->send_by_name(ctx, 0, ".launcher", message_type::PTYPE_TEXT, 0, (void*)"ERROR", 5);
}

static const char* optstring(service_context* ctx, const char* key, const char* str)
{
    const char* ret = service_command::handle_command(ctx, "GETENV", key);
    if (ret == NULL)
    {
        return str;
    }
    return ret;
}

// 初始化lua服务的回调, 设置一些虚拟机环境变量 (主要是路径资源之类的)
// 在init_cb里进行Lua层的初始化，比如初始化LUA_PATH，LUA_CPATH，LUA_SERVICE等全局变量
static int init_cb(struct snlua* l, service_context* ctx, const char* args, size_t sz)
{
    lua_State* L = l->L;
    l->svc_ctx = ctx;
    // 停止GC
    lua_gc(L, LUA_GCSTOP, 0);
    // 注册表["LUA_NOENV"] = true, 通知库忽略环境变量
    lua_pushboolean(L, 1);  /* signal for libraries to ignore env. vars. */
    lua_setfield(L, LUA_REGISTRYINDEX, "LUA_NOENV");
    luaL_openlibs(L);

    // 将ctx设置到LUA_REGISTRYINDEX里，以便在Lua中能获取到ctx
    lua_pushlightuserdata(L, ctx);
    lua_setfield(L, LUA_REGISTRYINDEX, "service_context");
    //
    luaL_requiref(L, "skynet.codecache", codecache, 0);
    lua_pop(L, 1);

    // lua脚本路径
    const char* path = optstring(ctx, "lua_path", "./lualib/?.lua;./lualib/?/init.lua");
    lua_pushstring(L, path);
    lua_setglobal(L, "LUA_PATH");

    // c库路径
    const char* cpath = optstring(ctx, "lua_cpath", "./luaclib/?.so");
    lua_pushstring(L, cpath);
    lua_setglobal(L, "LUA_CPATH");

    // lua服务路径
    const char* service = optstring(ctx, "luaservice", "./service/?.lua");
    lua_pushstring(L, service);
    lua_setglobal(L, "LUA_SERVICE");

    // 预加载, lua服务运行前执行, 设置全局变量LUA_PRELOAD
    const char* preload = service_command::handle_command(ctx, "GETENV", "preload");
    lua_pushstring(L, preload);
    lua_setglobal(L, "LUA_PRELOAD");

    // 设置堆栈跟踪函数
    lua_pushcfunction(L, traceback);
    assert(lua_gettop(L) == 1);

    // 加载loader.lua脚本
    const char* loader = optstring(ctx, "lualoader", "./lualib/loader.lua");
    int r = luaL_loadfile(L, loader);
    if (r != LUA_OK)
    {
        log(ctx, "Can't load %s : %s", (void*)loader, lua_tostring(L, -1));
        report_launcher_error(ctx);
        return 1;
    }
    // 运行loader.lua，参数是“bootstrap”, 即加载 bootstrap服务
    lua_pushlstring(L, args, sz);
    r = lua_pcall(L, 1, 0, 1);
    if (r != LUA_OK)
    {
        service_command::handle_command(ctx, "lua loader error : %s", lua_tostring(L, -1));
        report_launcher_error(ctx);
        return 1;
    }

    // 清空堆栈
    lua_settop(L, 0);

    // 虚拟机内存限制
    if (lua_getfield(L, LUA_REGISTRYINDEX, "memlimit") == LUA_TNUMBER)
    {
        size_t limit = lua_tointeger(L, -1);
        l->mem_limit = limit;
        log(ctx, "Set memory limit to %.2f M", (float)limit / (1024 * 1024));
        lua_pushnil(L);
        lua_setfield(L, LUA_REGISTRYINDEX, "memlimit");
    }
    lua_pop(L, 1);

    // 重启GC
    lua_gc(L, LUA_GCRESTART, 0);

    return 0;
}

// 消息回调函数
static int launch_cb(service_context* context, void* ud, int type, int session, uint32_t source, const void* msg, size_t sz)
{
    assert(type == 0 && session == 0);
    snlua* l = (snlua*)ud;

    // 将服务原本绑定的句柄和回调函数清空
    // 服务收到第一条消息后，先把消息回调函数至为NULL
    context->set_callback(NULL, NULL);

    // 设置各项资源路径参数, 并加载 loader.lua
    int err = init_cb(l, context, (const char*)msg, sz); // 在init_cb里进行Lua层的初始化，比如初始化LUA_PATH，LUA_CPATH，LUA_SERVICE等全局变量
    if (err)
    {
        service_command::handle_command(context, "EXIT", NULL);
    }

    return 0;
}

// lua 内存分配函数
static void* lalloc(void* ud, void* ptr, size_t osize, size_t nsize)
{
    // 记录分配信息
    snlua* l = (snlua*)ud;

    // 调整已用内存
    size_t mem = l->mem;
    l->mem += nsize;
    if (ptr)
        l->mem -= osize;
    // 检查内存分配限制
    if (l->mem_limit != 0 && l->mem > l->mem_limit)
    {
        if (ptr == NULL || nsize > osize)
        {
            l->mem = mem;
            return NULL;
        }
    }
    // 检查内存分配报告阈值
    if (l->mem > l->mem_report)
    {
        l->mem_report *= 2;
        log(l->svc_ctx, "Memory warning %.2f M", (float)l->mem / (1024 * 1024));
    }

    // 分配内存
    return skynet_lalloc(ptr, osize, nsize);
}

//-------------------------------------------
// mod interface
//-------------------------------------------

// 创建lua服务运行沙盒环境
snlua* snlua_create()
{
    snlua* l = (snlua*)skynet_malloc(sizeof(*l));
    memset(l, 0, sizeof(*l));
    l->mem_report = MEMORY_WARNING_REPORT;
    l->mem_limit = 0;
    // 创建一个lua虚拟机
    l->L = lua_newstate(lalloc, l);
    return l;
}

void snlua_release(struct snlua* l)
{
    lua_close(l->L);
    skynet_free(l);
}

// 这个方法里把服务自己在C语言层面的回调函数给注销了，使它不再接收消息，目的是：在lua层重新注册它，把消息通过lua接口来接收
int snlua_init(struct snlua* l, service_context* ctx, const char* args)
{
    // 在内存中准备一个空间(动态内存分配)
    int sz = strlen(args);
    char* tmp = new char[sz];
    // 将args内容拷贝到内存中的tmp指针指向地址的内存空间
    memcpy(tmp, args, sz);
    ctx->set_callback(l, launch_cb);        // 设置消息回调函数: launch_cb 这个函数, 有消息传入时会调用回调函数进行处理
    const char* self = service_command::handle_command(ctx, "REG", NULL);    //
    // 当前lua实例自己的句柄id (无符号长整型)
    uint32_t handle_id = strtoul(self + 1, NULL, 16);
    // it must be first message
    // 给自己发送一条消息, 内容为 args 字符串
    service_manager::instance()->send(ctx, 0, handle_id, message_type::TAG_DONT_COPY, 0, tmp, sz);

    return 0;
}

void snlua_signal(struct snlua* l, int signal)
{
    log(l->svc_ctx, "recv a signal %d", signal);
    if (signal == 0)
    {
#ifdef lua_checksig
        // If our lua support signal (modified lua version by skynet), trigger it.
        skynet_sig_L = l->L;
#endif
    }
    else if (signal == 1)
    {
        log(l->svc_ctx, "Current Memory %.3fK", (float)l->mem / 1024);
    }
}

} }
