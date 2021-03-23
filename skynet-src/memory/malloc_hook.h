/**
 * memory manage hook
 * 
 * used to service memory statistics
 */

#pragma once

#include <string>
#include <cstdlib>
#include <cstdbool>
#include <atomic>

// forward declare
struct lua_State;

//
extern std::atomic<size_t>  _used_memory;
extern std::atomic<size_t>  _memory_block;

// memory statistics info

struct mem_data
{
    std::atomic<uint32_t>   handle{ 0 };
    std::atomic<ssize_t>    allocated{ 0 };
};

#define SLOT_SIZE           0x10000
extern struct mem_data mem_stats[SLOT_SIZE];

//
extern size_t malloc_used_memory(void);
//
extern size_t malloc_memory_block(void);
//
extern void memory_info_dump(void);
//
extern size_t mallctl_int64(const char* name, size_t* new_value);
//
extern int mallctl_opt(const char* name, int* new_value);
//
extern bool mallctl_bool(const char* name, bool* new_value);
//
extern int mallctl_cmd(const char* name);

//
extern void dump_c_mem(void);
//
extern int dump_mem_lua(lua_State* L);
//
extern size_t malloc_current_memory(void);

// for debug use, output current service memory to stderr
extern void skynet_debug_memory(const char* info);

