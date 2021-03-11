/**
 * 内存管理钩子
 * 用于按服务分开统计内存的开销
 */

#pragma once

#include <stdlib.h>
#include <stdbool.h>

extern "C" {
#include <lua.h>
}

extern size_t malloc_used_memory(void);
extern size_t malloc_memory_block(void);
extern void   memory_info_dump(void);
extern size_t mallctl_int64(const char* name, size_t* newval);
extern int    mallctl_opt(const char* name, int* newval);
extern bool   mallctl_bool(const char* name, bool* newval);
extern int    mallctl_cmd(const char* name);
extern void   dump_c_mem(void);
extern int    dump_mem_lua(lua_State* L);
extern size_t malloc_current_memory(void);

