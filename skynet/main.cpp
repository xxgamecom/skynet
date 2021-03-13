#include "node/node.h"
#include "node/node_config.h"

#include "utils/signal_helper.h"

#include <iostream>

extern "C" {
#include <lauxlib.h>
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
    skynet::node::instance()->init();

    // load skynet node config
    std::string config_filename = argv[1];
    skynet::node_config cfg;
    if (!cfg.load(config_filename))
    {
        std::cerr << "load config file failed: " << config_filename << std::endl;
        return 1;
    }

    // start skynet node
    skynet::node::instance()->start(&cfg);

    // clean skynet node
    skynet::node::instance()->fini();

    return 0;
}
