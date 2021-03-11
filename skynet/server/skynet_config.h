#pragma once

#include <string>

namespace skynet {

// 配置信息
class skynet_config final
{
public:
    int                         thread_;                        // worker thread count (不要配置超过实际拥有的CPU核心数)
    int                         profile_;                       // enable/disable statistics (cpu cost each service), default enable

    const char*                 pid_file_;                      // daemon pid file, enable/disable daemon mode
                                                                // - enable:  pid_file = "./skynet.pid"
                                                                // - disable: pid_file = "" or 
                                                                //            pid_file = nil or 
                                                                //            not set this variable
                                                                // notice: need set logger when daemon mode enabled

    const char*                 cservice_path_;                 // C service module search path (.so search path)
    const char*                 bootstrap_;                     // skynet 启动的第一个服务以及其启动参数。默认配置为 snlua bootstrap ，即启动一个名为 bootstrap 的 lua 服务。通常指的是 service/bootstrap.lua 这段代码。
    
    const char*                 logger_;                        // log file, skynet_error() API 的日志输出文件
                                                                // - logger = nil or logger = "": 输出到stdout
                                                                // - logger = "./skynet.log": 输出到skynet.log日志文件中

    const char*                 log_service_;                   // log service, default "logger" 
                                                                // 你可以配置为你定制的 log 服务（比如加上时间戳等更多信息）。
                                                                // 可以参考 service_logger.c 来实现它。
                                                                // notice: 如果你希望用 lua 来编写这个服务，可以在这里填写 snlua ，然后在 logger 配置具体的 lua 服务的名字。
                                                                //         在 examples 目录下，有 config.userlog 这个范例可供参考。

public:
    // load config
    bool load(const std::string& config_file);
};

}
