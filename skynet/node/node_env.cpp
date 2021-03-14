#include "node_env.h"

extern "C" {
#include <lua.h>
#include <lauxlib.h>
}

namespace skynet {

node_env* node_env::instance_ = nullptr;

node_env* node_env::instance()
{
    static std::once_flag oc;
    std::call_once(oc, [&]() {
        instance_ = new node_env;
        instance_->L_ = luaL_newstate();
    });

    return instance_;
}

const char* node_env::get_env(const char* key)
{
    std::lock_guard<std::mutex> lock(mutex_);

    // from _ENV
    lua_getglobal(L_, key);
    const char* result = lua_tostring(L_, -1);
    
    // clear stack
    lua_pop(L_, 1);

    return result;
}

void node_env::set_env(const char* key, const char* value)
{
    std::lock_guard<std::mutex> lock(mutex_);

    // check old
    lua_getglobal(L_, key);
    assert(lua_isnil(L_, -1));	// 环境变量不可修改?
    lua_pop(L_,1);

    // set new
    lua_pushstring(L_,value);
    lua_setglobal(L_,key);
}

int node_env::get_int32(const char* key, int default_value)
{
    const char* str = get_env(key);

    // not exists, add default value
    if (str == nullptr) 
    {
        set_env(key, std::to_string(default_value).c_str());
        return default_value;
    }

    return std::stoi(str);
}

void node_env::set_int32(const char* key, int value)
{
    set_env(key, std::to_string(value).c_str());
}

int node_env::get_boolean(const char* key, int default_value)
{
    const char* str = get_env(key);

    // not exists, add default value
    if (str == nullptr)
    {
        set_env(key, default_value == 0 ? "false" : "true");
        return default_value;
    }

    return strcmp(str, "true") == 0;
}

void node_env::set_boolean(const char* key, int value)
{
    set_env(key, value == 0 ? "false" : "true" );
}

void node_env::set_boolean(const char* key, bool value)
{
    set_env(key, value ? "true" : "false" );
}

const char* node_env::get_string(const char* key, const char* default_value)
{
    const char* str = get_env(key);

    // not exists, add default value
    if (str == nullptr)
    {
        if (default_value != nullptr)
        {
            set_env(key, default_value);
            default_value = get_env(key);
        }
        return default_value;
    }

    return str;
}

void node_env::set_string(const char* key, const char* value)
{
    set_env(key, value);
}

}
