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

    // logger
public:
    int log_level_;                     // log level
    std::string log_basename_;          // log file basename
    std::string log_extension_;         // log file extension
    std::string log_dir_;               // log file directory

    const char* log_file_;              // log to file | log to console
                                        // - log_file = nil or log_file = "": log to console
                                        // - log_file = "./skynet.log": log to skynet.log

    const char* log_service_;           // log service or log file, default `logger`
                                        // 你可以配置为你定制的 log 服务（比如加上时间戳等更多信息）。
                                        // 可以参考 service_logger.c 来实现它。
                                        // notice: 如果你希望用 lua 来编写这个服务，可以在这里填写 snlua ，然后在 logger 配置具体的 lua 服务的名字。
                                        //         在 examples 目录下，有 config.userlog 这个范例可供参考。

public:
    // load config
    bool load(const std::string& config_file);


private:
    //
    bool load_log_config();
};

}
