#define LUA_LIB

//#include "malloc_hook.h"

extern "C" {
#include <lua.h>
#include <lauxlib.h>
}

static int ltotal(lua_State* L)
{
//    size_t t = malloc_used_memory();
    size_t t = 0;
    lua_pushinteger(L, (lua_Integer)t);

    return 1;
}

static int lblock(lua_State* L)
{
//    size_t t = malloc_memory_block();
    size_t t = 0;
    lua_pushinteger(L, (lua_Integer)t);

    return 1;
}

static int ldumpinfo(lua_State* L)
{
//    memory_info_dump();
    return 0;
}

static int
ldump(lua_State* L)
{
//    dump_c_mem();
    return 0;
}

static int lcurrent(lua_State* L)
{
//    size_t t = malloc_current_memory();
    size_t t = 0;
    lua_pushinteger(L, t);
    return 1;
}

static int ldumpheap(lua_State* L)
{
//    mallctl_cmd("prof.dump");
    return 0;
}

static int lprofactive(lua_State* L)
{
    bool* pval, active;
    if (lua_isnone(L, 1))
    {
        pval = NULL;
    }
    else
    {
        active = lua_toboolean(L, 1) ? true : false;
        pval = &active;
    }
//    bool ret = mallctl_bool("prof.active", pval);
    bool ret = false;
    lua_pushboolean(L, ret);
    return 1;
}

/**
 * skynet luaclib - skynet.memory
 */
#if __cplusplus
extern "C" {
#endif

LUAMOD_API int luaopen_skynet_memory(lua_State* L)
{
    luaL_checkversion(L);

    luaL_Reg l[] = {
        { "total",      ltotal },
        { "block",      lblock },
        { "dumpinfo",   ldumpinfo },
        { "dump",       ldump },
//        { "info",       dump_mem_lua },
        { "current",    lcurrent },
        { "dumpheap",   ldumpheap },
        { "profactive", lprofactive },
        { NULL, NULL },
    };

    luaL_newlib(L, l);

    return 1;
}

#if __cplusplus
}
#endif
