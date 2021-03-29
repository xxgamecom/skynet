#pragma once

#include <mutex>

struct lua_State;

namespace skynet {

// skynet node environment
class node_env final
{
    // singleton
private:
    static node_env* instance_;
public:
    static node_env* instance();

private:
    std::mutex                  mutex_;
    lua_State*                  L_ = nullptr;               // lua VM used for skynet node env

public:
    // set/get node environment variable
    void set_env(const char* key, const char* value);
    const char* get_env(const char* key);

public:
    // set/get node environment variable - int32
    void set_int32(const char* key, int value);
    int get_int32(const char* key, int default_value);

    // set/get node environment variable - boolean
    void set_boolean(const char* key, int value);
    void set_boolean(const char* key, bool value);    
    int get_boolean(const char* key, int default_value);

    // set/get node environment variable - string
    void set_string(const char* key, const char* value);
    const char* get_string(const char* key, const char* default_value);

};

}

