#pragma once

namespace skynet {

// 配置信息
struct skynet_config
{
    int thread;                     // 工作线程数量，不要配置超过实际拥有的CPU核心数
    int profile;                    // 是否开启性能统计功能，统计每个服务使用了多少cpu时间，默认开启
    const char* daemon;             // daemon pid file, daemon = "./skynet.pid"可以以后台模式启动skynet（注意，同时请配置logger 项输出log）
    const char* module_path;        // 用 C 编写的服务模块的位置，通常指 cservice 下那些 .so 文件
    const char* bootstrap;          // skynet 启动的第一个服务以及其启动参数。默认配置为 snlua bootstrap ，即启动一个名为 bootstrap 的 lua 服务。通常指的是 service/bootstrap.lua 这段代码。
    const char* logger;             // 日志文件, 决定了 skynet 内建的 skynet_error 这个 C API 将信息输出到什么文件中。如果 logger 配置为 nil ，将输出到标准输出。你可以配置一个文件名来将信息记录在特定文件中。
    const char* logservice;         // 默认为 "logger" ，你可以配置为你定制的 log 服务（比如加上时间戳等更多信息）。可以参考 service_logger.c 来实现它。注：如果你希望用 lua 来编写这个服务，可以在这里填写 snlua ，然后在 logger 配置具体的 lua 服务的名字。在 examples 目录下，有 config.userlog 这个范例可供参考。
};

}
