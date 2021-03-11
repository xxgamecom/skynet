#include "server/env.h"
#include "server/skynet_config.h"
#include "server/skynet_node.h"

#include "utils/signal_helper.h"

#include <iostream>
#include <cassert>
#include <csignal>

extern "C" {
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
}


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
            skynet::env::instance()->set_boolean(key, b);
        }
        else
        {
            const char* value = lua_tostring(L, -1);
            if (value == nullptr)
            {
                std::cerr << "Invalid config table key =" << key << std::endl;
                exit(1);
            }

            skynet::env::instance()->set_string(key, value);
        }

        // clean, 保留key做下一次迭代
        lua_pop(L, 1);
    }

    // clean
    lua_pop(L,1);
}

// 加载配置的lua脚本
static const char* load_config_lua = "\
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

static bool load_config(const char* config_file, skynet::skynet_config& config)
{
    // 读取skynet配置数据 (使用临时的lua虚拟机读取, 读完后关闭)
    lua_State* L = luaL_newstate();
    luaL_openlibs(L);

    // 执行加载节点配置lua脚本字符串
    int err =  luaL_loadbufferx(L, load_config_lua, ::strlen(load_config_lua), "=[skynet config]", "t");
    assert(err == LUA_OK);

    // 加载配置文件
    lua_pushstring(L, config_file);
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
    config.thread = skynet::env::instance()->get_int32("thread", 8);                            // work thread数量
    config.module_path = skynet::env::instance()->get_string("cpath", "./cservice/?.so");       // c服务路径
    config.bootstrap = skynet::env::instance()->get_string("bootstrap","snlua bootstrap");      // bootstrap服务
    config.daemon = skynet::env::instance()->get_string("daemon", nullptr);                     // 后台模式
    config.logger = skynet::env::instance()->get_string("logger", nullptr);                     // skynet_error输出文件
    config.logservice = skynet::env::instance()->get_string("logservice", "logger");            // skynet_error输出logger服务, 可以使用外部自定义的lua logger服务
    config.profile = skynet::env::instance()->get_boolean("profile", 1);                        // 

    // 读完后关闭上面创建的Lua虚拟机
    lua_close(L);

    return true;
}

int main(int argc, char* argv[])
{
    // 命令行至少要有一个skynet配置文件
    if (argc <= 1)
    {
        std::cerr << "Need a config file. Please read skynet wiki : https://github.com/cloudwu/skynet/wiki/Config" << std::endl;
        std::cerr << "usage: skynet configfilename" << std::endl;
        return 1;
    }

    // ignore SIGPIPE
    skynet::signal_helper::ignore_sigpipe();

    // initialize lua code cache
#ifdef LUA_CACHELIB
    luaL_initcodecache();
#endif

	// initialize skynet node
    skynet::skynet_node::instance()->init();

    // load skynet node config
    const char* config_file = argv[1];
    skynet::skynet_config config;
    if (!load_config(config_file, config))
        return 1;

    // start skynet node
    skynet::skynet_node::instance()->start(&config);

    // clean skynet node
    skynet::skynet_node::instance()->fini();

    return 0;
}
