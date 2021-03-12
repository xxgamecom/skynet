#include "node_config.h"
#include "node_env.h"

#include <iostream>

extern "C" {
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
}

namespace skynet {


// initialize skynet node env
static void _init_env(lua_State* L)
{
    // first key 
    lua_pushnil(L);

    // table放在索引 -2 处, 获取表的第一个key和value，压入栈
    while (lua_next(L, -2) != 0)
    {
        // 下一个key在-2处, value在-1处
        int keyt = lua_type(L, -2);
        if (keyt != LUA_TSTRING)
        {
            std::cerr << "Invalid config table" << std::endl;
            exit(1);
        }

        const char* key = lua_tostring(L, -2);
        if (lua_type(L, -1) == LUA_TBOOLEAN)
        {
            int b = lua_toboolean(L, -1);
            skynet::node_env::instance()->set_boolean(key, b);
        }
        else
        {
            const char* value = lua_tostring(L, -1);
            if (value == nullptr)
            {
                std::cerr << "Invalid config table key =" << key << std::endl;
                exit(1);
            }

            skynet::node_env::instance()->set_string(key, value);
        }

        // clean, 保留key做下一次迭代
        lua_pop(L, 1);
    }

    // clean
    lua_pop(L,1);
}

// 加载配置的lua脚本
static const char* lua_script = "\
    local result = {}\n\
    local function getenv(name)\n\
        return assert(os.getenv(name), [[os.getenv() failed: ]] .. name)\n\
    end\n\
    local sep = package.config:sub(1,1)\n\
    local current_path = [[.]]..sep\n\
    local function include(filename)\n\
        local last_path = current_path\n\
        local path, name = filename:match([[(.*]]..sep..[[)(.*)$]])\n\
        if path then\n\
            if path:sub(1,1) == sep then	-- root\n\
                current_path = path\n\
            else\n\
                current_path = current_path .. path\n\
            end\n\
        else\n\
            name = filename\n\
        end\n\
        local f = assert(io.open(current_path .. name))\n\
        local code = assert(f:read [[*a]])\n\
        code = string.gsub(code, [[%$([%w_%d]+)]], getenv)\n\
        f:close()\n\
        assert(load(code,[[@]]..filename,[[t]],result))()\n\
        current_path = last_path\n\
    end\n\
    setmetatable(result, { __index = { include = include } })\n\
    local config_name = ...\n\
    include(config_name)\n\
    setmetatable(result, nil)\n\
    return result\n\
";

bool node_config::load(const std::string& config_file)
{
    // 读取skynet配置数据 (使用临时的lua虚拟机读取, 读完后关闭)
    lua_State* L = luaL_newstate();
    luaL_openlibs(L);

    // 执行加载节点配置lua脚本字符串
    int err =  luaL_loadbufferx(L, lua_script, ::strlen(lua_script), "=[skynet config]", "t");
    assert(err == LUA_OK);

    // 加载配置文件
    lua_pushstring(L, config_file.c_str());
    err = lua_pcall(L, 1, 1, 0);
    if (err != 0)
    {
        std::cerr << lua_tostring(L, -1) << std::endl;
        lua_close(L);
        return false;
    }

    // 初始化skyent节点环境(读取配置文件的环境配置)
    _init_env(L);

    // 从节点环境中读取配置数据
    thread_ = skynet::node_env::instance()->get_int32("thread", 8);                                // work thread count
    cservice_path_ = skynet::node_env::instance()->get_string("cservice_path", "./cservice/?.so"); // c service mod search path
    bootstrap_ = skynet::node_env::instance()->get_string("bootstrap","snlua bootstrap");          // bootstrap服务
    pid_file_ = skynet::node_env::instance()->get_string("pid_file", nullptr);                     // enable/disable daemon mode
    logger_ = skynet::node_env::instance()->get_string("logger", nullptr);                         // log output file
    log_service_ = skynet::node_env::instance()->get_string("log_service", "logger");              // skynet_error输出logger服务, 可以使用外部自定义的lua logger服务
    profile_ = skynet::node_env::instance()->get_boolean("profile", 1);                            // enable/disable statistics

    // close VM
    lua_close(L);

    return true;
}

}
