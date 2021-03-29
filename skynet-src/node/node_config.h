#pragma once

#include <string>

namespace skynet {

// skynet node config
class node_config final
{
    // base info
public:
    int thread_;                        // worker thread count (不要配置超过实际拥有的CPU核心数)
    int profile_;                       // enable/disable statistics (cpu cost each service), default enable

    const char* daemon_pid_file_;       // daemon pid file, enable/disable daemon mode
                                        // - enable:  daemon = "./skynet.pid"
                                        // - disable: daemon = "" or
                                        //            daemon = nil or
                                        //            not set this variable
                                        // notice: need set logger when daemon mode enabled

    const char* cservice_path_;         // C service module search path (.so search path)
    const char* bootstrap_;             // skynet 启动的第一个服务以及其启动参数。默认配置为 snlua bootstrap ，即启动一个名为 bootstrap 的 lua 服务。通常指的是 service/bootstrap.lua 这段代码。

public:
    // load config
    bool load(const std::string& config_file);
};

}
