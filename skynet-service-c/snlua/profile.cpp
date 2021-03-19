#include "profile.h"
#include "snlua_mod.h"

extern "C" {
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
}

#if defined(__APPLE__)

#include <mach/task.h>
#include <mach/mach.h>

#endif

namespace skynet { namespace service {

// #define DEBUG_LOG

#define NANOSEC 1000000000
#define MICROSEC 1000000

static double get_time()
{
#if !defined(__APPLE__)
    struct timespec ti;
    clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ti);

    int sec = ti.tv_sec & 0xffff;
    int nsec = ti.tv_nsec;

    return (double)sec + (double)nsec / NANOSEC;
#else
    struct task_thread_times_info aTaskInfo;
    mach_msg_type_number_t aTaskInfoCount = TASK_THREAD_TIMES_INFO_COUNT;
    if (KERN_SUCCESS != task_info(mach_task_self(), TASK_THREAD_TIMES_INFO, (task_info_t)&aTaskInfo, &aTaskInfoCount))
    {
        return 0;
    }

    int sec = aTaskInfo.user_time.seconds & 0xffff;
    int msec = aTaskInfo.user_time.microseconds;

    return (double)sec + (double)msec / MICROSEC;
#endif
}

static void switchL(lua_State* L, snlua_mod* mod_ptr)
{
    mod_ptr->activeL = L;
    if (mod_ptr->trap)
    {
        lua_sethook(L, signal_hook, LUA_MASKCOUNT, 1);
    }
}

static int lua_resumeX(lua_State* L, lua_State* from, int nargs, int* nresults)
{
    void* ud = nullptr;
    lua_getallocf(L, &ud);
    auto mod_ptr = (snlua_mod*)ud;

    //
    switchL(L, mod_ptr);

    int err = lua_resume(L, from, nargs, nresults);
    if (mod_ptr->trap)
    {
        // wait for lua_sethook. (mod_ptr->trap == -1)
        while (mod_ptr->trap >= 0);
    }

    switchL(from, mod_ptr);

    return err;
}

static inline double diff_time(double start)
{
    double now = get_time();
    if (now < start)
    {
        return now + 0x10000 - start;
    }
    else
    {
        return now - start;
    }
}

// coroutine lib, add profile

/*
** Resumes a coroutine. Returns the number of results for non-error
** cases or -1 for errors.
*/
static int auxresume(lua_State* L, lua_State* co, int narg)
{
    int status, nres;
    if (!lua_checkstack(co, narg))
    {
        lua_pushliteral(L, "too many arguments to resume");
        return -1;  /* error flag */
    }
    lua_xmove(L, co, narg);
    status = lua_resumeX(co, L, narg, &nres);
    if (status == LUA_OK || status == LUA_YIELD)
    {
        if (!lua_checkstack(L, nres + 1))
        {
            lua_pop(co, nres);  /* remove results anyway */
            lua_pushliteral(L, "too many results to resume");
            return -1;  /* error flag */
        }
        lua_xmove(co, L, nres);  /* move yielded values */
        return nres;
    }
    else
    {
        lua_xmove(co, L, 1);  /* move error message */
        return -1;  /* error flag */
    }
}

static int timing_enable(lua_State* L, int co_index, lua_Number* start_time)
{
    lua_pushvalue(L, co_index);
    lua_rawget(L, lua_upvalueindex(1));
    if (lua_isnil(L, -1))
    {        // check total time
        lua_pop(L, 1);
        return 0;
    }
    *start_time = lua_tonumber(L, -1);
    lua_pop(L, 1);

    return 1;
}

static double timing_total(lua_State* L, int co_index)
{
    lua_pushvalue(L, co_index);
    lua_rawget(L, lua_upvalueindex(2));
    double total_time = lua_tonumber(L, -1);
    lua_pop(L, 1);
    return total_time;
}

static int timing_resume(lua_State* L, int co_index, int n)
{
    lua_State* co = lua_tothread(L, co_index);
    lua_Number start_time = 0;
    if (timing_enable(L, co_index, &start_time))
    {
        start_time = get_time();
#ifdef DEBUG_LOG
        ::fprintf(stderr, "PROFILE [%p] resume %lf\n", co, ti);
#endif
        lua_pushvalue(L, co_index);
        lua_pushnumber(L, start_time);
        lua_rawset(L, lua_upvalueindex(1));    // set start time
    }

    int r = auxresume(L, co, n);

    if (timing_enable(L, co_index, &start_time))
    {
        double total_time = timing_total(L, co_index);
        double diff = diff_time(start_time);
        total_time += diff;
#ifdef DEBUG_LOG
        ::fprintf(stderr, "PROFILE [%p] yield (%lf/%lf)\n", co, diff, total_time);
#endif
        lua_pushvalue(L, co_index);
        lua_pushnumber(L, total_time);
        lua_rawset(L, lua_upvalueindex(2));
    }

    return r;
}


/**
 * start or result a coroutine
 */
static int luaB_coresume(lua_State* L)
{
    luaL_checktype(L, 1, LUA_TTHREAD);
    int r = timing_resume(L, 1, lua_gettop(L) - 1);
    if (r < 0)
    {
        lua_pushboolean(L, 0);
        lua_insert(L, -2);
        return 2;  /* return false + error message */
    }
    else
    {
        lua_pushboolean(L, 1);
        lua_insert(L, -(r + 1));
        return r + 1;  /* return true + 'resume' returns */
    }
}

static int luaB_auxwrap(lua_State* L)
{
    lua_State* co = lua_tothread(L, lua_upvalueindex(3));
    int r = timing_resume(L, lua_upvalueindex(3), lua_gettop(L));
    if (r < 0)
    {
        int stat = lua_status(co);
        if (stat != LUA_OK && stat != LUA_YIELD)
            lua_resetthread(co);  /* close variables in case of errors */
        if (lua_type(L, -1) == LUA_TSTRING)
        {  /* error object is a string? */
            luaL_where(L, 1);  /* add extra info, if available */
            lua_insert(L, -2);
            lua_concat(L, 2);
        }
        return lua_error(L);  /* propagate error */
    }
    return r;
}

static int luaB_cocreate(lua_State* L)
{
    lua_State* NL;
    luaL_checktype(L, 1, LUA_TFUNCTION);
    NL = lua_newthread(L);
    lua_pushvalue(L, 1);  /* move function to top */
    lua_xmove(L, NL, 1);  /* move function from L to NL */
    return 1;
}

static int luaB_cowrap(lua_State* L)
{
    lua_pushvalue(L, lua_upvalueindex(1));
    lua_pushvalue(L, lua_upvalueindex(2));
    luaB_cocreate(L);
    lua_pushcclosure(L, luaB_auxwrap, 3);
    return 1;
}

// profile lib

static int l_start(lua_State* L)
{
    if (lua_gettop(L) != 0)
    {
        lua_settop(L, 1);
        luaL_checktype(L, 1, LUA_TTHREAD);
    }
    else
    {
        lua_pushthread(L);
    }
    lua_Number start_time = 0;
    if (timing_enable(L, 1, &start_time))
    {
        return luaL_error(L, "Thread %p start profile more than once", lua_topointer(L, 1));
    }

    // reset total time
    lua_pushvalue(L, 1);
    lua_pushnumber(L, 0);
    lua_rawset(L, lua_upvalueindex(2));

    // set start time
    lua_pushvalue(L, 1);
    start_time = get_time();
#ifdef DEBUG_LOG
    ::fprintf(stderr, "PROFILE [%p] start\n", L);
#endif
    lua_pushnumber(L, start_time);
    lua_rawset(L, lua_upvalueindex(1));

    return 0;
}

static int l_stop(lua_State* L)
{
    if (lua_gettop(L) != 0)
    {
        lua_settop(L, 1);
        luaL_checktype(L, 1, LUA_TTHREAD);
    }
    else
    {
        lua_pushthread(L);
    }
    lua_Number start_time = 0;
    if (!timing_enable(L, 1, &start_time))
    {
        return luaL_error(L, "Call profile.start() before profile.stop()");
    }
    double ti = diff_time(start_time);
    double total_time = timing_total(L, 1);

    lua_pushvalue(L, 1);    // push coroutine
    lua_pushnil(L);
    lua_rawset(L, lua_upvalueindex(1));

    lua_pushvalue(L, 1);    // push coroutine
    lua_pushnil(L);
    lua_rawset(L, lua_upvalueindex(2));

    total_time += ti;
    lua_pushnumber(L, total_time);
#ifdef DEBUG_LOG
    ::fprintf(stderr, "PROFILE [%p] stop (%lf/%lf)\n", lua_tothread(L,1), ti, total_time);
#endif

    return 1;
}

void signal_hook(lua_State* L, lua_Debug* ar)
{
    void* ud = nullptr;
    lua_getallocf(L, &ud);
    auto mod_ptr = (snlua_mod*)ud;

    lua_sethook(L, nullptr, 0, 0);
    if (mod_ptr->trap)
    {
        mod_ptr->trap = 0;
        luaL_error(L, "signal 0");
    }
}

int init_profile(lua_State* L)
{
    luaL_Reg profile_funcs[] = {
        { "start",  l_start },
        { "stop",   l_stop },
        { "resume", luaB_coresume },
        { "wrap",   luaB_cowrap },

        { nullptr,  nullptr },
    };

    luaL_newlibtable(L, profile_funcs);
    lua_newtable(L);    // table thread->start time
    lua_newtable(L);    // table thread->total time

    lua_newtable(L);    // weak table
    lua_pushliteral(L, "kv");
    lua_setfield(L, -2, "__mode");

    lua_pushvalue(L, -1);
    lua_setmetatable(L, -3);
    lua_setmetatable(L, -3);

    luaL_setfuncs(L, profile_funcs, 2);

    return 1;
}

/// end of coroutine

} }

