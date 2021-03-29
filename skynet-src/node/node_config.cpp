#include "node_config.h"
#include "node_env.h"

#include <iostream>
#include <cassert>
#include <cstring>

extern "C" {
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
}

namespace skynet {


// initialize skynet node env (load config)
static void _init_env(lua_State* L, std::string parent_key = "")
{
    // first key
    lua_pushnil(L);
    // table position: -2
    while (lua_next(L, -2) != 0) /* after call next, key position: -2, value position: -1 */
    {
        // key type must string
        int key_type = lua_type(L, -2);
        if (key_type != LUA_TSTRING)
        {
            std::cerr << "Invalid config table" << std::endl;
            exit(1);
        }
        // get key
        std::string key = lua_tostring(L, -2);

        // get value
        int value_type = lua_type(L, -1);
        // boolean value
        if (value_type == LUA_TBOOLEAN)
        {
            int value = lua_toboolean(L, -1);
            if (!parent_key.empty())
            {
                key = parent_key + "_" + key;
            }
            skynet::node_env::instance()->set_boolean(key.c_str(), value);
        }
        // table value
        else if (value_type == LUA_TTABLE)
        {
            if (!parent_key.empty())
            {
                key = parent_key + "_" + key;
            }
            _init_env(L, key);
        }
        // other
        else
        {
            const char* value = lua_tostring(L, -1);
            if (value == nullptr)
            {
                std::cerr << "Invalid config table, key=" << key << std::endl;
                exit(1);
            }

            if (!parent_key.empty())
            {
                key = parent_key + "_" + key;
            }
            skynet::node_env::instance()->set_string(key.c_str(), value);
        }

        // pop value & keep key in stack for next iteration
        lua_pop(L, 1);
    }

    // clean
    if (parent_key.empty())
    {
        lua_pop(L,1);
    }
}

// load config lua script
static const char* lua_script = "\
    local result = {}\n\
    local function getenv(name)\n\
        return assert(os.getenv(name), [[os.getenv() failed: ]] .. name)\n\
    end\n\
    local sep = package.config:sub(1, 1)\n\
    local current_path = [[.]] .. sep\n\
    local function include(filename)\n\
        local last_path = current_path\n\
        local path, name = filename:match([[(.*]] .. sep .. [[)(.*)$]])\n\
        if path then\n\
            if path:sub(1, 1) == sep then    -- root\n\
                current_path = path\n\
            else\n\
                current_path = current_path .. path\n\
            end\n\
        else\n\
            name = filename\n\
        end\n\
        local f = assert(io.open(current_path .. name))\n\
        local code = assert(f:read([[*a]]))\n\
        code = string.gsub(code, [[%$([%w_%d]+)]], getenv)\n\
        f:close()\n\
        assert(load(code, [[@]] .. filename, [[t]], result))()\n\
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

    // load lua_script
    int err = luaL_loadbufferx(L, lua_script, ::strlen(lua_script), "=[skynet config]", "t");
    assert(err == LUA_OK);

    // load config file
    lua_pushstring(L, config_file.c_str());
    err = lua_pcall(L, 1, 1, 0);
    if (err != 0)
    {
        std::cerr << lua_tostring(L, -1) << std::endl;
        lua_close(L);
        return false;
    }

    // init node env (read from config file)
    _init_env(L);

    // close VM
    lua_close(L);

    // read config from node env
    thread_ = skynet::node_env::instance()->get_int32("thread", 8);                                 // work thread count
    cservice_path_ = skynet::node_env::instance()->get_string("cservice_path", "./cservice/?.so");  // c service mod search path
    bootstrap_ = skynet::node_env::instance()->get_string("bootstrap","snlua bootstrap");           // bootstrap服务
    daemon_pid_file_ = skynet::node_env::instance()->get_string("daemon", nullptr);                 // enable/disable daemon mode
    profile_ = skynet::node_env::instance()->get_boolean("profile", 1);                             // enable/disable statistics

    // log config
    load_log_config();

    return true;
}

bool node_config::load_log_config()
{
    // log output file
    log_file_ = skynet::node_env::instance()->get_string("log_file", nullptr);
    // skynet::log输出logger服务, 可以使用外部自定义的lua logger服务
    log_service_ = skynet::node_env::instance()->get_string("log_service", "logger");
}

}
