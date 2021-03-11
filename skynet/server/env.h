#pragma once

extern "C" {
#include <lua.h>
}

#include <mutex>

namespace skynet {

// skynet env variables
class env
{
    // singleton
private:
    static env* instance_;
public:
    static env* instance();

private:
    std::mutex                  mutex_;
    lua_State*                  L_ = nullptr;

public:
    env() = default;
    ~env() = default;

public:
    /**
     * 获取环境变量
     * @param key
     * @return
     */
    const char* getenv(const char* key);

    /**
     * 设置环境变量
     * @param key
     * @param value
     */
    void setenv(const char* key, const char* value);

public:
    /**
     * 获取环境变量 (整数)
     * @param key 环境变量key
     * @param default_value 默认值
     */
    int get_int32(const char* key, int default_value);

    /**
     * 设置环境变量 (整数)
     * @param key 环境变量key
     * @param value 环境变量值
     */
    void set_int32(const char* key, int value);

    /**
     * 获取环境变量 (布尔值)
     * @param key 环境变量key
     * @param default_value 默认值
     */
    int get_boolean(const char* key, int default_value);

    /**
     * 设置环境变量 (布尔值)
     * @param key 环境变量key
     * @param value 环境变量值
     */    
    void set_boolean(const char* key, int value);
    void set_boolean(const char* key, bool value);

    /**
     * 获取环境变量 (字符串)
     * @param key 环境变量key
     * @param default_value 默认值
     */
    const char* get_string(const char* key, const char* default_value);

    /**
     * 设置环境变量 (字符串)
     * @param key 环境变量key
     * @param value 环境变量值
     */
    void set_string(const char* key, const char* value);

};

}
