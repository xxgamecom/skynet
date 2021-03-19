#pragma once

#include <atomic>

// forward delcare
struct lua_State;

namespace skynet {
class service_context;
}

namespace skynet { namespace service {

#define MEMORY_WARNING_REPORT (1024 * 1024 * 32)

// snlua c service own data
struct snlua_mod
{
    lua_State*                  L = nullptr;
    service_context*            svc_ctx = nullptr;
    size_t                      mem = 0;                            // 已占用内存
    size_t                      mem_report = MEMORY_WARNING_REPORT; // 内存报告阈值
    size_t                      mem_limit = 0;                      // 最大内存分配限制值
    lua_State*                  activeL = nullptr;                  //
    std::atomic<int>            trap { 0 };                      //
};

} }

