#include <string>
#include <cassert>
#include <cstdlib>

extern "C" {
#include <lua.h>
}

#include "malloc_hook.h"
#include "../skynet.h"

//
std::atomic<size_t> _used_memory = 0;
std::atomic<size_t> _memory_block = 0;

// memory statistics info
struct mem_data mem_stats[SLOT_SIZE];

size_t malloc_used_memory(void)
{
    return _used_memory;
}

size_t malloc_memory_block(void)
{
    return _memory_block;
}

void dump_c_mem()
{
    size_t total = 0;
    skynet::log(NULL, "dump all service mem:");
    for (int i = 0; i < SLOT_SIZE; i++)
    {
        struct mem_data* data = &mem_stats[i];
        if (data->handle != 0 && data->allocated != 0)
        {
            total += data->allocated;
            skynet::log(NULL, ":%08x -> %zdkb %db", data->handle.load(), data->allocated.load() >> 10, (int) (data->allocated % 1024));
        }
    }
    skynet::log(NULL, "+total: %zdkb", total >> 10);
}

int dump_mem_lua(lua_State* L)
{
    lua_newtable(L);
    for (int i = 0; i < SLOT_SIZE; i++)
    {
        struct mem_data* data = &mem_stats[i];
        if (data->handle != 0 && data->allocated != 0)
        {
            lua_pushinteger(L, data->allocated);
            lua_rawseti(L, -2, (lua_Integer) data->handle);
        }
    }

    return 1;
}

size_t malloc_current_memory(void)
{
//    uint32_t handle = skynet_current_handle();
    uint32_t handle;
    for (int i = 0; i < SLOT_SIZE; i++)
    {
        struct mem_data* data = &mem_stats[i];
        if (data->handle == (uint32_t) handle && data->allocated != 0)
        {
            return (size_t) data->allocated;
        }
    }

    return 0;
}

void skynet_debug_memory(const char* info)
{
    // for debug use
//    uint32_t handle = skynet_current_handle();
    uint32_t handle;
    size_t mem = malloc_current_memory();
    ::fprintf(stderr, "[:%08x] %s %p\n", handle, info, (void*) mem);
}


