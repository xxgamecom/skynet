#define LUA_LIB

extern "C" {
#include <lua.h>
#include <lauxlib.h>
}

#include <cstdlib>
#include <cstdint>
#include <cassert>
#include <cstring>

#include <atomic>
#include <shared_mutex>

struct stm_copy
{
    std::atomic<int32_t> reference;
    uint32_t sz;
    void* msg;
};

struct stm_object
{
    std::shared_mutex rw_mutex;
    std::atomic<int32_t> reference;
    stm_copy* copy;
};

// msg should alloc by skynet_malloc
static stm_copy* stm_newcopy(void* msg, int32_t sz)
{
    stm_copy* copy = new stm_copy;
    copy->reference = 1;
    copy->sz = sz;
    copy->msg = msg;

    return copy;
}

static stm_object* stm_new(void* msg, int32_t sz)
{
    stm_object* obj = new stm_object;
    obj->reference = 1;
    obj->copy = stm_newcopy(msg, sz);

    return obj;
}

static void stm_releasecopy(stm_copy* copy)
{
    if (copy == nullptr)
        return;
    if (copy->reference-- <= 1)
    {
        delete[] copy->msg;
        delete copy;
    }
}

static void stm_release(struct stm_object* obj)
{
    assert(obj->copy);
    std::unique_lock<std::shared_mutex> wlock(obj->rw_mutex);

    // writer release the stm object, so release the last copy .
    stm_releasecopy(obj->copy);
    obj->copy = nullptr;
    if (obj->reference-- > 1)
    {
        // stm object grab by readers, reset the copy to NULL.
        return;
    }

    // no one grab the stm object, no need to unlock wlock.
    delete obj;
}

static void stm_releasereader(struct stm_object* obj)
{
    std::shared_lock<std::shared_mutex> rlock(obj->rw_mutex);

    if (obj->reference-- == 1)
    {
        // last reader, no writer. so no need to unlock
        assert(obj->copy == nullptr);
        delete obj;
        return;
    }
}

static void stm_grab(struct stm_object* obj)
{
    std::shared_lock<std::shared_mutex> rlock(obj->rw_mutex);

    int ref = obj->reference++;
    assert(ref > 0);
}

static stm_copy* stm_copy(stm_object* obj)
{
    std::shared_lock<std::shared_mutex> rlock(obj->rw_mutex);

    struct stm_copy * ret = obj->copy;
    if (ret)
    {
        int ref = ret->reference++;
        assert(ref > 0);
    }

    return ret;
}

static void stm_update(struct stm_object* obj, void* msg, int32_t sz)
{
    struct stm_copy* copy = stm_newcopy(msg, sz);

    std::unique_lock<std::shared_mutex> wlock(obj->rw_mutex);

    struct stm_copy* oldcopy = obj->copy;
    obj->copy = copy;

    stm_releasecopy(oldcopy);
}

// lua binding

struct boxstm
{
    stm_object* obj;
};

static int l_copy(lua_State* L)
{
    boxstm* box = (boxstm*)lua_touserdata(L, 1);
    stm_grab(box->obj);
    lua_pushlightuserdata(L, box->obj);
    return 1;
}

static int l_new_writer(lua_State* L)
{
    void* msg;
    size_t sz;
    if (lua_isuserdata(L, 1))
    {
        msg = lua_touserdata(L, 1);
        sz = (size_t)luaL_checkinteger(L, 2);
    }
    else
    {
        const char* tmp = luaL_checklstring(L, 1, &sz);
        msg = new char[sz];
        memcpy(msg, tmp, sz);
    }
    boxstm* box = (boxstm*)lua_newuserdata(L, sizeof(*box));
    box->obj = stm_new(msg, sz);
    lua_pushvalue(L, lua_upvalueindex(1));
    lua_setmetatable(L, -2);

    return 1;
}

static int l_delete_writer(lua_State* L)
{
    boxstm* box = (boxstm*)lua_touserdata(L, 1);
    stm_release(box->obj);
    box->obj = nullptr;

    return 0;
}

static int l_update(lua_State* L)
{
    boxstm* box = (boxstm*)lua_touserdata(L, 1);
    void* msg;
    size_t sz;
    if (lua_isuserdata(L, 2))
    {
        msg = lua_touserdata(L, 2);
        sz = (size_t)luaL_checkinteger(L, 3);
    }
    else
    {
        const char* tmp = luaL_checklstring(L, 2, &sz);
        msg = new char[sz];
        memcpy(msg, tmp, sz);
    }
    stm_update(box->obj, msg, sz);

    return 0;
}

struct boxreader
{
    stm_object* obj;
    struct stm_copy* lastcopy;
};

static int l_new_reader(lua_State* L)
{
    boxreader* box = (boxreader*)lua_newuserdata(L, sizeof(*box));
    box->obj = (stm_object*)lua_touserdata(L, 1);
    box->lastcopy = nullptr;
    lua_pushvalue(L, lua_upvalueindex(1));
    lua_setmetatable(L, -2);

    return 1;
}

static int l_delete_reader(lua_State* L)
{
    boxreader* box = (boxreader*)lua_touserdata(L, 1);
    stm_releasereader(box->obj);
    box->obj = nullptr;
    stm_releasecopy(box->lastcopy);
    box->lastcopy = nullptr;

    return 0;
}

static int l_read(lua_State* L)
{
    boxreader* box = (boxreader*)lua_touserdata(L, 1);
    luaL_checktype(L, 2, LUA_TFUNCTION);

    struct stm_copy* copy = stm_copy(box->obj);
    if (copy == box->lastcopy)
    {
        // not update
        stm_releasecopy(copy);
        lua_pushboolean(L, 0);
        return 1;
    }

    stm_releasecopy(box->lastcopy);
    box->lastcopy = copy;
    if (copy)
    {
        lua_settop(L, 3);
        lua_replace(L, 1);
        lua_settop(L, 2);
        lua_pushlightuserdata(L, copy->msg);
        lua_pushinteger(L, copy->sz);
        lua_pushvalue(L, 1);
        lua_call(L, 3, LUA_MULTRET);
        lua_pushboolean(L, 1);
        lua_replace(L, 1);
        return lua_gettop(L);
    }
    else
    {
        lua_pushboolean(L, 0);
        return 1;
    }
}

/**
 * skynet luaclib - skynet.stm
 */

#if __cplusplus
extern "C" {
#endif

LUAMOD_API int luaopen_skynet_stm(lua_State* L)
{
    luaL_checkversion(L);

    lua_createtable(L, 0, 3);

    //
    lua_pushcfunction(L, l_copy);
    lua_setfield(L, -2, "copy");

    //
    luaL_Reg writer[] = {
        { "new",   l_new_writer },
        { nullptr, nullptr },
    };
    lua_createtable(L, 0, 2);
    lua_pushcfunction(L, l_delete_writer), lua_setfield(L, -2, "__gc");
    lua_pushcfunction(L, l_update), lua_setfield(L, -2, "__call");
    luaL_setfuncs(L, writer, 1);

    //
    luaL_Reg reader[] = {
        { "newcopy", l_new_reader },
        { nullptr,   nullptr },
    };
    lua_createtable(L, 0, 2);
    lua_pushcfunction(L, l_delete_reader), lua_setfield(L, -2, "__gc");
    lua_pushcfunction(L, l_read), lua_setfield(L, -2, "__call");
    luaL_setfuncs(L, reader, 1);

    return 1;
}

#if __cplusplus
}
#endif

