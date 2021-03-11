#include "env.h"

#include <mutex>

extern "C" {
#include <lauxlib.h>
}

namespace skynet {

env* env::instance_ = nullptr;

env* env::instance()
{
    static std::once_flag oc;
    std::call_once(oc, [&]() {
        instance_ = new env;
        instance_->L_ = luaL_newstate();
    });

    return instance_;
}

const char* env::getenv(const char* key)
{
    std::lock_guard<std::mutex> lock(mutex_);

    // from _ENV
    lua_getglobal(L_, key);
    const char* result = lua_tostring(L_, -1);
    
    // clear stack
    lua_pop(L_, 1);

    return result;
}

void env::setenv(const char* key, const char* value)
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

int env::get_int32(const char* key, int default_value)
{
    const char* str = getenv(key);

    // not exists, add default value
    if (str == nullptr) 
    {
        this->setenv(key, std::to_string(default_value).c_str());
        return default_value;
    }

    return std::stoi(str);
}

void env::set_int32(const char* key, int value)
{
    this->setenv(key, std::to_string(value).c_str());
}

int env::get_boolean(const char* key, int default_value)
{
    const char* str = this->getenv(key);

    // not exists, add default value
    if (str == nullptr)
    {
        this->setenv(key, default_value == 0 ? "false" : "true");
        return default_value;
    }

    return strcmp(str, "true") == 0;
}

void env::set_boolean(const char* key, int value)
{
    this->setenv(key, value == 0 ? "false" : "true" );
}

void env::set_boolean(const char* key, bool value)
{
    this->setenv(key, value ? "true" : "false" );
}

const char* env::get_string(const char* key, const char* default_value)
{
    const char* str = this->getenv(key);

    // not exists, add default value
    if (str == nullptr)
    {
        if (default_value != nullptr)
        {
            this->setenv(key, default_value);
            default_value = this->getenv(key);
        }
        return default_value;
    }

    return str;
}

void env::set_string(const char* key, const char* value)
{
    this->setenv(key, value);
}

}
