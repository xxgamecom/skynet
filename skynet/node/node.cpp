#include "node.h"
#include "node_thread.h"
#include "node_config.h"
#include "skynet_socket.h"
#include "server.h"

#include "../mq/mq.h"
#include "../log/log.h"
#include "../mod/cservice_mod_manager.h"
#include "../timer/timer_manager.h"
#include "../context/handle_manager.h"
#include "../context/service_context.h"

#include "../utils/daemon_helper.h"

#include <iostream>
#include <mutex>

namespace skynet {

node* node::instance_ = nullptr;

node* node::instance()
{
    static std::once_flag oc;
    std::call_once(oc, [&]() {
        instance_ = new node;
    });

    return instance_;
}

bool node::init(const std::string config_filename)
{
    // initialize skynet node lua code cache
#ifdef LUA_CACHELIB
    luaL_initcodecache();
#endif

    // load skynet node config
    if (!node_config_.load(config_filename))
    {
        std::cerr << "load node config file failed: " << config_filename << std::endl;
        return false;
    }

    //
    total_ = 0;
    monitor_exit_ = 0;
    init_ = 1;

    return true;
}

void node::fini()
{
}


// 启动 lua bootstrap服务
// snlua bootstrap
static void _bootstrap(service_context* log_svc_ctx, const char* cmdline)
{
    // 命令行长度
    int sz = ::strlen(cmdline);
    char svc_name[sz + 1];
    char svc_args[sz + 1];
    // 命令行字符串按照格式分割成两部分, 前部分为服务模块名, 后部分为服务模块初始化参数
    ::sscanf(cmdline, "%s %s", svc_name, svc_args);

    // create service
    service_context* ctx = skynet_context_new(svc_name, svc_args);
    if (ctx == nullptr)
    {
        // 通过传入的logger服务接口构建错误信息加入logger的消息队列
        log(nullptr, "Bootstrap error : %s\n", cmdline);
        // 输出消息队列中的错误信息
        skynet_context_dispatchall(log_svc_ctx);

        ::exit(1);
    }
}

void node::start()
{
    // daemon mode
    if (node_config_.pid_file_ != nullptr)
    {
        if (!daemon_helper::init(node_config_.pid_file_))
        {
            ::exit(1);
        }
    }

    // 初始化具柄模块, 用于每个skynet服务创建一个全局唯一的具柄值
    handle_manager::instance()->init();

    // 初始化消息队列
    global_mq::instance()->init();

    // 初始化服务动态库加载模块, 主要用户加载符合skynet服务模块接口的动态链接库(.so文件)
    cservice_mod_manager::instance()->init(node_config_.cservice_path_);

    // 初始化定时器
    timer_manager::instance()->init();

    // 初始化网络模块
    skynet_socket_init();

    // enable/disable profiler
    node::instance()->enable_profiler(node_config_.profile_);

    // create c service: logger
    service_context* log_svc_ctx = skynet_context_new(node_config_.log_service_, node_config_.logger_);
    if (log_svc_ctx == nullptr)
    {
        std::cerr << "Can't launch " << node_config_.log_service_ << " service" << std::endl;
        ::exit(1);
    }
    handle_manager::instance()->set_handle_by_name("logger", log_svc_ctx->svc_handle_);

    // bootstrap to load snlua c service
    _bootstrap(log_svc_ctx, node_config_.bootstrap_);

    // start server threads
    node_thread::start(node_config_.thread_);

    //
    skynet_socket_free();

    // clean daemon pid file
    if (node_config_.pid_file_)
    {
        daemon_helper::fini(node_config_.pid_file_);
    }
}

}


