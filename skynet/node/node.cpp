#include "node.h"
#include "node_thread.h"
#include "node_config.h"
#include "node_socket.h"

#include "../mq/mq_msg.h"
#include "../mq/mq_private.h"
#include "../mq/mq_global.h"

#include "../log/log.h"

#include "../timer/timer_manager.h"

#include "../service/service_log.h"
#include "../service/service_context.h"
#include "../service/service_monitor.h"
#include "../service/service_manager.h"

#include "../mod/service_mod_manager.h"

#include "../utils/daemon_helper.h"
#include "../utils/time_helper.h"

#include <iostream>
#include <mutex>

namespace skynet {

node* node::instance_ = nullptr;

node* node::instance()
{
    static std::once_flag oc;
    std::call_once(oc, [&]() { instance_ = new node; });

    return instance_;
}

//
struct drop_t
{
    uint32_t svc_handle;
};

static void drop_message(skynet_message* msg, void* ud)
{
    drop_t* d = (drop_t*)ud;
    delete[] msg->data;

    uint32_t src_svc_handle = d->svc_handle;
    assert(src_svc_handle != 0);

    // report error to the message source
    service_manager::instance()->send(nullptr, src_svc_handle, msg->src_svc_handle, message_protocol_type::PTYPE_ERROR, 0, nullptr, 0);
}

bool node::init(const std::string config_filename)
{
    // initialize skynet node lua code cache
#ifdef LUA_CACHELIB
    luaL_initcodecache();
#endif

    // load skynet node config
    if (!config_.load(config_filename))
    {
        std::cerr << "load node config file failed: " << config_filename << std::endl;
        return false;
    }

    //
    monitor_exit_ = 0;

    return true;
}

void node::fini()
{
}

void node::start()
{
    // daemon mode
    if (config_.pid_file_ != nullptr)
    {
        if (!daemon_helper::init(config_.pid_file_))
        {
            ::exit(1);
        }
    }

    //
    service_manager::instance()->init();
    //
    mq_global::instance()->init();
    //
    service_mod_manager::instance()->init(config_.cservice_path_);
    //
    timer_manager::instance()->init();
    //
    node_socket::instance()->init();

    // enable/disable profiler
    enable_profiler(config_.profile_);

    // create c service: logger
    service_context* log_svc_ctx = service_manager::instance()->create_service(config_.log_service_, config_.logger_);
    if (log_svc_ctx == nullptr)
    {
        std::cerr << "Can't launch " << config_.log_service_ << " service" << std::endl;
        ::exit(1);
    }
    service_manager::instance()->set_handle_by_name("logger", log_svc_ctx->svc_handle_);

    // bootstrap to load snlua c service
    _bootstrap(log_svc_ctx, config_.bootstrap_);

    // start server threads
    node_thread::start(config_.thread_);

    //
    node_socket::instance()->fini();

    // clean daemon pid file
    if (config_.pid_file_)
    {
        daemon_helper::fini(config_.pid_file_);
    }
}

mq_private* node::dispatch_message(service_monitor& svc_monitor, mq_private* q, int weight)
{
    // peek next q from global mq
    if (q == nullptr)
        q = mq_global::instance()->pop();

    // no more message
    if (q == nullptr)
        return nullptr;

    // service handle
    uint32_t svc_handle = q->svc_handle_;

    service_context* svc_ctx = service_manager::instance()->grab(svc_handle);
    // service context not exists
    if (svc_ctx == nullptr)
    {
        struct drop_t d = { svc_handle };
        q->release(drop_message, &d);
        return mq_global::instance()->pop();
    }

    // handle message
    skynet_message msg;
    int n = 1;
    for (int i = 0; i < n; i++)
    {
        // pop a private message
        bool is_empty = q->pop(&msg);

        // service private queue is empty
        if (is_empty)
        {
            service_manager::instance()->release_service(svc_ctx);
            return mq_global::instance()->pop();
        }

        // weight: -1 process one message;
        //          0 process all messages;
        //         >0 process 1 / (2 ^ weight) messages.
        if (i == 0 && weight >= 0)
        {
            n = q->length();
            n >>= weight;
        }

        // check overload, just log
        int overload = q->overload();
        if (overload != 0)
        {
            log(svc_ctx, "May overload, message queue length = %d", overload);
        }

        // process message
        {
            // tell service monitor, that the service start handle messages.
            svc_monitor.process_begin(msg.src_svc_handle , svc_handle);

            if (svc_ctx->cb_ == nullptr)
                delete[] msg.data;
            else
                _do_dispatch_message(svc_ctx, &msg);

            // tell service monitor, that the service has handle messages.
            svc_monitor.process_end();
        }
    }

    // next private queue
    assert(q == svc_ctx->queue_);
    mq_private* nq = mq_global::instance()->pop();
    if (nq != nullptr)
    {
        // If global mq is not empty, push q back, and return next queue (nq);
        // Else (global mq is empty or block, don't push q back, and return q again (for next dispatch).
        mq_global::instance()->push(q);
        q = nq;
    }

    service_manager::instance()->release_service(svc_ctx);

    return q;
}


// 启动 lua bootstrap 服务
// snlua bootstrap
void node::_bootstrap(service_context* log_svc_ctx, const char* cmdline)
{
    // 命令行长度
    int sz = ::strlen(cmdline);
    char svc_name[sz + 1];
    char svc_args[sz + 1];
    // 命令行字符串按照格式分割成两部分, 前部分为服务模块名, 后部分为服务模块初始化参数
    ::sscanf(cmdline, "%s %s", svc_name, svc_args);

    // create service
    service_context* svc_ctx = service_manager::instance()->create_service(svc_name, svc_args);
    if (svc_ctx == nullptr)
    {
        // 通过传入的logger服务接口构建错误信息加入logger的消息队列
        log(nullptr, "Bootstrap error : %s\n", cmdline);
        // 输出消息队列中的错误信息
        _dispatch_all(log_svc_ctx);

        ::exit(1);
    }
}

void node::_dispatch_all(service_context* svc_ctx)
{
    // for log
    skynet_message msg;
    mq_private* q = svc_ctx->queue_;

    // dispatch all
    while (!q->pop(&msg))
    {
        _do_dispatch_message(svc_ctx, &msg);
    }
}

// handle service message (call service message callback)
void node::_do_dispatch_message(service_context* svc_ctx, skynet_message* msg)
{
    int type = msg->sz >> MESSAGE_TYPE_SHIFT;
    size_t sz = msg->sz & MESSAGE_TYPE_MASK;
    if (svc_ctx->log_fd_ != nullptr)
    {
        service_log::log(svc_ctx->log_fd_, msg->src_svc_handle, type, msg->session, msg->data, sz);
    }

    //
    ++svc_ctx->message_count_;

    int reserve_msg = 0;
    if (svc_ctx->profile_)
    {
        svc_ctx->cpu_start_ = time_helper::thread_time();

        // message callback
        reserve_msg = svc_ctx->cb_(svc_ctx, svc_ctx->cb_ud_, type, msg->session, msg->src_svc_handle, msg->data, sz);

        uint64_t cost_time = time_helper::thread_time() - svc_ctx->cpu_start_;
        svc_ctx->cpu_cost_ += cost_time;
    }
    else
    {
        // message callback
        reserve_msg = svc_ctx->cb_(svc_ctx, svc_ctx->cb_ud_, type, msg->session, msg->src_svc_handle, msg->data, sz);
    }

    //
    if (reserve_msg == 0)
    {
        delete[] msg->data;
    }
}

}


