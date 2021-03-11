#include "skynet_node.h"
#include "skynet_node_thread.h"

#include "skynet_config.h"
#include "skynet_socket.h"

#include "../skynet.h"
#include "../mq/mq.h"
#include "../mod/module_manager.h"
#include "../timer/timer_manager.h"
#include "../context/handle_manager.h"

#include "../utils/daemon_helper.h"

#include <iostream>
#include <mutex>

namespace skynet {

skynet_node* skynet_node::instance_ = nullptr;

skynet_node* skynet_node::instance()
{
    static std::once_flag oc;
    std::call_once(oc, [&](){ 
        instance_ = new skynet_node;
    });

    return instance_;
}

void skynet_node::init()
{
    // 初始化节点
    total_ = 0;
    monitor_exit_ = 0;
    init_ = 1;
}

void skynet_node::fini()
{
}


// 启动 lua bootstrap服务
// snlua bootstrap
static void bootstrap(skynet_context* log_svc_ctx, const char* cmdline)
{
    // 命令行长度
    int sz = ::strlen(cmdline);
    char svc_name[sz + 1];
    char svc_args[sz + 1];
    // 命令行字符串按照格式分割成两部分, 前部分为服务模块名, 后部分为服务模块初始化参数
    ::sscanf(cmdline, "%s %s", svc_name, svc_args);

    // create service
//     skynet_context* ctx = skynet_context_new(svc_name, svc_args);
//     if (ctx == nullptr)
    {
//         // 通过传入的logger服务接口构建错误信息加入logger的消息队列
//         skynet_error(nullptr, "Bootstrap error : %s\n", cmdline);
//         // 输出消息队列中的错误信息
//         skynet_context_dispatchall(log_svc_ctx);

        ::exit(1);
    }
}

void skynet_node::start(skynet_config* config)
{
    // daemon mode
    if (config->pid_file_ != nullptr)
    {
        if (daemon_helper::init(config->pid_file_))
        {
            ::exit(1);
        }
    }

    // 初始化具柄模块, 用于每个skynet服务创建一个全局唯一的具柄值
    handle_manager::instance()->init();

    // 初始化消息队列
    global_mq::instance()->init();

    // 初始化服务动态库加载模块, 主要用户加载符合skynet服务模块接口的动态链接库(.so文件)
    module_manager::instance()->init(config->cservice_path_);

    // 初始化定时器
    timer_manager::instance()->init();

    // 初始化网络模块
    skynet_socket_init();

    // enable/disable profiler
    skynet_node::instance()->profile_enable(config->profile_);

    // create c service: logger
//     skynet_context* log_svc_ctx = skynet_context_new(config->log_service_, config->logger_);
//     if (log_svc_ctx == nullptr)
    {
        std::cerr << "Can't launch " << config->log_service_ << " service" << std::endl;
        ::exit(1);
    }
    // handle_manager::instance()->set_handle_by_name("logger", log_svc_ctx->handle);

    // bootstrap to load snlua c service
    // bootstrap(log_svc_ctx, config->bootstrap);

    // start server threads
    server_thread::start(config->thread_);

    //
    skynet_socket_free();

    // clean daemon pid file
    if (config->pid_file_)
    {
        daemon_helper::fini(config->pid_file_);
    }
}

}


