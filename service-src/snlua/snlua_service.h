#pragma once

#include "skynet.h"

#include <cstdint>
#include <atomic>

// forward declare
struct lua_State;

namespace skynet {
class service_context;
}

namespace skynet { namespace service {

/**
 * snlua_service c service mod
 * skynet lua service sandbox
 *
 * skynet绝大部分服务类型是snlua，它是运行Lua脚本的服务，
 * 在用skynet框架上开发游戏服务器时，大部分逻辑都是snlua服务，90%以上只需写Lua代码即可
 */
class snlua_service : public cservice
{
public:
    enum
    {
        MEMORY_WARNING_REPORT       = 32 * 1024 * 1024,                     //
    };

public:
    lua_State*                      L_ = nullptr;                           //
    service_context*                svc_ctx_ = nullptr;                     //
    uint64_t                        mem_ = 0;                               // used memory
    uint64_t                        mem_report_ = MEMORY_WARNING_REPORT;    //
    uint64_t                        mem_limit_ = 0;                         //

public:
    snlua_service();
    virtual ~snlua_service();

    // cservice impl
public:
    //
    bool init(service_context* svc_ctx, const char* param) override;
    //
    void fini() override;
    //
    void signal(int signal) override;

public:
    // service message callback
    static int snlua_cb(service_context* svc_ctx, void* ud, int msg_ptype, int session_id, uint32_t src_svc_handle, const void* msg, size_t sz);
    // init lua service message callback
    // 设置一些虚拟机环境变量 (主要是路径资源之类的)
    // 在init_cb里进行Lua层的初始化，比如初始化LUA_PATH，LUA_CPATH，LUA_SERVICE等全局变量
    static bool init_lua_cb(snlua_service* svc_ptr, service_context* svc_ctx, const char* args, size_t sz);

    // lua memory alloc
    static void* lalloc(void* ud, void* ptr, size_t osize, size_t nsize);
};

} }


