#define LUA_LIB

extern "C" {
#include <lua.h>
#include <lauxlib.h>
}

#if defined(__APPLE__)
#include <mach/task.h>
#include <mach/mach.h>
#endif

#include <ctime>

// #define DEBUG_LOG

#define NANO_PER_SEC 1000000000
#define MICRO_PER_SEC 1000000

// high-resolution timer provided by the CPU for each of the threads.
// 本线程到当前代码系统CPU花费的时间 (seconds)
static double _get_profile_time()
{
#if defined(__APPLE__)
    task_thread_times_info the_task_info;
    mach_msg_type_number_t task_info_count = TASK_THREAD_TIMES_INFO_COUNT;
    if (task_info(mach_task_self(), TASK_THREAD_TIMES_INFO, (task_info_t)&the_task_info, &task_info_count) != KERN_SUCCESS)
    {
        return 0;
    }

    int sec = the_task_info.user_time.seconds & 0xffff; // only need 2 bytes
    int us = the_task_info.user_time.microseconds;

    return (double)sec + (double)us / MICRO_PER_SEC;
#else
    timespec ti;
    clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ti);

    int sec = ti.tv_sec & 0xffff; // only need 2 bytes
    int nsec = ti.tv_nsec;

    return (double)sec + (double)nsec / NANO_PER_SEC;
#endif
}

static inline double _diff_time(double start_sec)
{
    double now_sec = _get_profile_time();
    if (now_sec < start_sec)
    {
        return now_sec + 0x10000 - start_sec;
    }
    else
    {
        return now_sec - start_sec;
    }
}

static int _timing_resume(lua_State* L)
{
    lua_pushvalue(L, -1);
    lua_rawget(L, lua_upvalueindex(2));
    if (lua_isnil(L, -1)) // check total time
    {
        lua_pop(L, 2);    // pop from coroutine
    }
    else
    {
        lua_pop(L, 1);
        double now_sec = _get_profile_time();
#ifdef DEBUG_LOG
        ::fprintf(stderr, "PROFILE [%p] resume %lf\n", lua_tothread(L, -1), ti);
#endif
        // set start time
        lua_pushnumber(L, now_sec);
        lua_rawset(L, lua_upvalueindex(1));
    }

    // coroutine.resume
    lua_CFunction co_resume = lua_tocfunction(L, lua_upvalueindex(3));
    return co_resume(L);
}

//
static int _timing_yield(lua_State* L)
{
#ifdef DEBUG_LOG
    lua_State* from = lua_tothread(L, -1);
#endif
    lua_pushvalue(L, -1);
    lua_rawget(L, lua_upvalueindex(2));    // check total time
    if (lua_isnil(L, -1))
    {
        lua_pop(L, 2);
    }
    else
    {
        double ti = lua_tonumber(L, -1);
        lua_pop(L, 1);

        lua_pushvalue(L, -1);    // push coroutine
        lua_rawget(L, lua_upvalueindex(1));
        double start_sec = lua_tonumber(L, -1);
        lua_pop(L, 1);

        double diff_sec = _diff_time(start_sec);
        ti += diff_sec;
#ifdef DEBUG_LOG
        ::fprintf(stderr, "PROFILE [%p] yield (%lf/%lf)\n", from, diff, ti);
#endif

        lua_pushvalue(L, -1);      // push coroutine
        lua_pushnumber(L, ti);
        lua_rawset(L, lua_upvalueindex(2));
        lua_pop(L, 1);              // pop coroutine
    }

    // coroutine.yield
    lua_CFunction co_yield = lua_tocfunction(L, lua_upvalueindex(3));
    return co_yield(L);
}

/**
 * start profile
 * record coroutine start exec time
 *
 * arguments:
 * 1 coroutine              - thread (optional)
 *
 * lua examples:
 * local profile = require "skynet.profile"
 * profile.start()
 * ...
 * local ti = profile.stop()
 * ...
 */
static int l_start(lua_State* L)
{
    // have arguments
    if (lua_gettop(L) != 0)
    {
        // first argument must be a coroutine
        lua_settop(L, 1);
        luaL_checktype(L, 1, LUA_TTHREAD);
    }
    // no arguments
    else
    {
        lua_pushthread(L);
    }

    // push coroutine
    lua_pushvalue(L, 1);
    lua_rawget(L, lua_upvalueindex(2));
    if (!lua_isnil(L, -1))
    {
        return luaL_error(L, "Thread %p start profile more than once", lua_topointer(L, 1));
    }

    // push coroutine
    lua_pushvalue(L, 1);
    lua_pushnumber(L, 0);
    lua_rawset(L, lua_upvalueindex(2));

    // push coroutine
    lua_pushvalue(L, 1);
    double now_sec = _get_profile_time();
#ifdef DEBUG_LOG
    ::fprintf(stderr, "PROFILE [%p] start\n", L);
#endif
    lua_pushnumber(L, now_sec);
    lua_rawset(L, lua_upvalueindex(1));

    return 0;
}

/**
 * stop profile
 * record coroutine end exec time
 *
 * arguments:
 * 1 coroutine                      - thread (optional)
 *
 * outputs:
 * coroutine end exec time (ticks)  - integer
 *
 * lua examples:
 * local profile = require "skynet.profile"
 * profile.start()
 * ...
 * local ti = profile.stop()
 * ...
 */
static int l_stop(lua_State* L)
{
    // have argument
    if (lua_gettop(L) != 0)
    {
        // first argument must be a coroutine
        lua_settop(L, 1);
        luaL_checktype(L, 1, LUA_TTHREAD);
    }
    else
    {
        lua_pushthread(L);
    }

    // push coroutine
    lua_pushvalue(L, 1);
    lua_rawget(L, lua_upvalueindex(1));
    if (lua_type(L, -1) != LUA_TNUMBER)
    {
        return luaL_error(L, "Call profile.start() before profile.stop()");
    }

    //
    double start_sec = lua_tonumber(L, -1);
    double diff_sec = _diff_time(start_sec);

    // push coroutine
    lua_pushvalue(L, 1);
    lua_rawget(L, lua_upvalueindex(2));
    double total_time_sec = lua_tonumber(L, -1);

    // push coroutine
    lua_pushvalue(L, 1);
    lua_pushnil(L);
    lua_rawset(L, lua_upvalueindex(1));

    // push coroutine
    lua_pushvalue(L, 1);
    lua_pushnil(L);
    lua_rawset(L, lua_upvalueindex(2));

    // return coroutine exec ticks
    total_time_sec += diff_sec;
    lua_pushnumber(L, total_time_sec);
#ifdef DEBUG_LOG
    ::fprintf(stderr, "PROFILE [%p] stop (%lf/%lf)\n", lua_tothread(L,1), ti, total_time_sec);
#endif

    return 1;
}

/**
 *
 * arguments:
 * 1 coroutine                    - thread
 * 2 coroutine fucntion arguments -
 *
 * lua examples:
 * local profile = require "skynet.profile"
 * profile_resume = profile.resume
 * profile_resume(co, ...)
 */
static int l_resume(lua_State* L)
{
    // dup thread object & push to stack top
    lua_pushvalue(L, 1);

    return _timing_resume(L);
}

static int l_yield(lua_State* L)
{
    lua_pushthread(L);

    return _timing_yield(L);
}

static int l_resume_co(lua_State* L)
{
    luaL_checktype(L, 2, LUA_TTHREAD);
    lua_rotate(L, 2, -1);    // 'from' coroutine rotate to the top(index -1)

    return _timing_resume(L);
}

static int l_yield_co(lua_State* L)
{
    luaL_checktype(L, 1, LUA_TTHREAD);
    lua_rotate(L, 1, -1);

    return _timing_yield(L);
}

/**
 * skynet luaclib - skynet.profile
 */

#if __cplusplus
extern "C" {
#endif

LUAMOD_API int luaopen_skynet_profile(lua_State* L)
{
    luaL_checkversion(L);

    luaL_Reg l[] = {
        { "start",     l_start },
        { "stop",      l_stop },
        { "resume",    l_resume },
        { "yield",     l_yield },
        { "resume_co", l_resume_co },
        { "yield_co",  l_yield_co },

        { nullptr,     nullptr },
    };
    luaL_newlibtable(L, l);

    // t1 = setmetatable({}, { __mode="kv" })
    // t2 = setmetatable({}, { __mode="kv" })
    lua_newtable(L);    // table thread->start time
    lua_newtable(L);    // table thread->total time

    lua_newtable(L);    // weak table
    lua_pushliteral(L, "kv");
    lua_setfield(L, -2, "__mode");

    lua_pushvalue(L, -1);
    lua_setmetatable(L, -3);
    lua_setmetatable(L, -3);

    // 3 upvalue: t1, t2, nil
    lua_pushnil(L);    // cfunction (coroutine.resume or coroutine.yield)
    luaL_setfuncs(L, l, 3);

    int libtable = lua_gettop(L);

    // get coroutine.resume
    lua_getglobal(L, "coroutine");
    lua_getfield(L, -1, "resume");
    lua_CFunction co_resume = lua_tocfunction(L, -1);
    if (co_resume == nullptr)
    {
        return luaL_error(L, "Can't get coroutine.resume");
    }
    lua_pop(L, 1);

    //
    lua_getfield(L, libtable, "resume");
    lua_pushcfunction(L, co_resume);
    lua_setupvalue(L, -2, 3);
    lua_pop(L, 1);

    lua_getfield(L, libtable, "resume_co");
    lua_pushcfunction(L, co_resume);
    lua_setupvalue(L, -2, 3);
    lua_pop(L, 1);

    // get coroutine.yield
    lua_getfield(L, -1, "yield");
    lua_CFunction co_yield = lua_tocfunction(L, -1);
    if (co_yield == nullptr)
    {
        return luaL_error(L, "Can't get coroutine.yield");
    }
    lua_pop(L, 1);

    //
    lua_getfield(L, libtable, "yield");
    lua_pushcfunction(L, co_yield);
    lua_setupvalue(L, -2, 3);
    lua_pop(L, 1);

    //
    lua_getfield(L, libtable, "yield_co");
    lua_pushcfunction(L, co_yield);
    lua_setupvalue(L, -2, 3);
    lua_pop(L, 1);

    lua_settop(L, libtable);

    return 1;
}

#if __cplusplus
}
#endif
