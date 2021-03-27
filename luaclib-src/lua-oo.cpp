// OO support, from tolua++
#define LUA_LIB

extern "C" {
#include <lua.h>
#include <lauxlib.h>
}

#include <cstdlib>
#include <cstring>

static int _push_table_instance(lua_State* L, int lo)
{
    if (lua_istable(L, lo))
    {
        lua_pushstring(L, ".c_instance");
        lua_gettable(L, lo);
        if (lua_isuserdata(L, -1))
        {
            lua_replace(L, lo);
            return 1;
        }
        else
        {
            lua_pop(L, 1);
            return 0;
        }
    }

    return 0;
}

/* the equivalent of lua_is* for usertype */
int lua_isusertype(lua_State* L, int lo, const char* type)
{
    if (lua_isuserdata(L, lo) == 0)
    {
        if (_push_table_instance(L, lo) == 0)
        {
            return 0;
        }
    }

    /* check if it is of the same type */
    if (lua_getmetatable(L, lo))        /* if metatable? */
    {
        lua_rawget(L, LUA_REGISTRYINDEX);  /* get registry[mt] */
        const char* tn = lua_tostring(L, -1);
        bool r = tn && (::strcmp(tn, type) == 0);
        lua_pop(L, 1);
        if (r)
        {
            return 1;
        }

        /* check if it is a specialized class */
        lua_pushstring(L, "tolua_super");
        lua_rawget(L, LUA_REGISTRYINDEX); /* get super */
        lua_getmetatable(L, lo);
        lua_rawget(L, -2);                /* get super[mt] */
        if (lua_istable(L, -1))
        {
            lua_pushstring(L, type);
            lua_rawget(L, -2);                /* get super[mt][type] */
            int b = lua_toboolean(L, -1);
            lua_pop(L, 3);
            if (b)
            {
                return 1;
            }
        }
    }

    return 0;
}

static int _isusertype(lua_State* L, int lo, const char* type, int def)
{
    if (def && lua_gettop(L) < abs(lo))
        return 1;
    if (lua_isnil(L, lo) || lua_isusertype(L, lo, type))
        return 1;

    return 0;
}

static int l_bnd_setpeer(lua_State* L)
{
    /* stack: userdata, table */
    if (!lua_isuserdata(L, -2))
    {
        lua_pushstring(L, "Invalid argument #1 to setpeer: userdata expected.");
        lua_error(L);
    }

    if (lua_isnil(L, -1))
    {
        lua_pop(L, 1);
        lua_pushvalue(L, LUA_REGISTRYINDEX);
    }
    lua_setuservalue(L, -2);

    return 0;
};

static int l_bnd_getpeer(lua_State* L)
{
    /* stack: userdata */
    lua_getuservalue(L, -1);
    if (lua_rawequal(L, -1, LUA_REGISTRYINDEX))
    {
        lua_pop(L, 1);
        lua_pushnil(L);
    }
    return 1;
};

static int l_bnd_iskindof(lua_State* L)
{
    const char* type = luaL_checkstring(L, 2);
    if (lua_gettop(L) < 2)
    {
        lua_pushstring(L, "Miss arguments to iskindof.");
        lua_error(L);
    }

    if (lua_getmetatable(L, 1) == 0)
    {
        lua_pushstring(L, "Invalid argument #1 to iskindof: class or object expected.");
        lua_error(L);
    }

    if (type == nullptr)
    {
        lua_pushstring(L, "Invalid argument #2 to iskindof: string expected.");
        lua_error(L);
    }

    lua_pushboolean(L, _isusertype(L, 1, type, 0));
    return 1;
}

/**
 * skynet luaclib - skynet.core
 */

#if __cplusplus
extern "C" {
#endif

LUAMOD_API int luaopen_skynet_oo(lua_State* L)
{
    luaL_checkversion(L);

    luaL_Reg oo_funcs[] = {
        { "getpeer", l_bnd_getpeer },
        { "setpeer", l_bnd_setpeer },
        { "iskindof", l_bnd_iskindof },

        { nullptr,   nullptr },
    };

    luaL_newlib(L, oo_funcs);

    return 1;
}

#if __cplusplus
}
#endif
