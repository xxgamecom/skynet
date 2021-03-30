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

#include "../mod/mod_manager.h"

#include "../utils/daemon_helper.h"
#include "../utils/time_helper.h"

#include <regex>
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

static void drop_message(service_message* msg, void* ud)
{
    drop_t* d = (drop_t*)ud;
    delete[] msg->data_ptr;

    uint32_t src_svc_handle = d->svc_handle;
    assert(src_svc_handle != 0);

    // report error to the message source
    service_manager::instance()->send(nullptr, src_svc_handle, msg->src_svc_handle, SERVICE_MSG_TYPE_ERROR, 0, nullptr, 0);
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
    if (config_.daemon_pid_file_ != nullptr)
    {
        if (!daemon_helper::init(config_.daemon_pid_file_))
        {
            ::exit(1);
        }
    }

    //
    service_manager::instance()->init();
    //
    mq_global::instance()->init();
    //
    mod_manager::instance()->init(config_.cservice_path_);
    //
    timer_manager::instance()->init();
    //
    node_socket::instance()->init();

    // enable/disable profiler
    enable_profiler(config_.profile_);

    // create c service: logger
    service_context* log_svc_ctx = service_manager::instance()->create_service("logger", nullptr);
    if (log_svc_ctx == nullptr)
    {
        std::cerr << "Can't launch logger service" << std::endl;
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
    if (config_.daemon_pid_file_)
    {
        daemon_helper::fini(config_.daemon_pid_file_);
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
    service_message msg;
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
            log_warn(svc_ctx, fmt::format("May overload, message queue length = {}", overload));
        }

        // process message
        {
            // tell service monitor, that the service start handle messages.
            svc_monitor.process_begin(msg.src_svc_handle , svc_handle);

            if (svc_ctx->msg_callback_ == nullptr)
            {
                delete[] msg.data_ptr;
            }
            else
            {
                _do_dispatch_message(svc_ctx, &msg);
            }

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


// use snlua to start lua service `bootstrap`
void node::_bootstrap(service_context* log_svc_ctx, const char* cmdline)
{
    std::string cmdline_string = cmdline;

    // split command line by ' '
    std::regex re {' '};
    std::vector<std::string> cmdline_info {
        std::sregex_token_iterator(cmdline_string.begin(), cmdline_string.end(), re, -1),
        std::sregex_token_iterator()
    };

    // check command line
    if (cmdline_info.size() < 2)
    {
        log_error(nullptr, fmt::format("Bootstrap error : {}", cmdline));
        // output all error message in mq
        _dispatch_all(log_svc_ctx);

        ::exit(1);
    }

    // create service
    std::string svc_name = cmdline_info[0];
    std::string svc_args = cmdline_info[1];
    service_context* svc_ctx = service_manager::instance()->create_service(svc_name.c_str(), svc_args.c_str());
    if (svc_ctx == nullptr)
    {
        log_error(nullptr, fmt::format("Bootstrap error : {}", cmdline));
        // output all error message in mq
        _dispatch_all(log_svc_ctx);

        ::exit(1);
    }
}

void node::_dispatch_all(service_context* svc_ctx)
{
    // for log
    service_message msg;
    mq_private* q = svc_ctx->queue_;

    // dispatch all
    while (!q->pop(&msg))
    {
        _do_dispatch_message(svc_ctx, &msg);
    }
}

// handle service message (call service message callback)
void node::_do_dispatch_message(service_context* svc_ctx, service_message* msg)
{
    int svc_msg_type = msg->data_size >> MESSAGE_TYPE_SHIFT;
    size_t msg_sz = msg->data_size & MESSAGE_TYPE_MASK;
    if (svc_ctx->log_fd_ != nullptr)
    {
        service_log::log(svc_ctx->log_fd_, msg->src_svc_handle, svc_msg_type, msg->session_id, msg->data_ptr, msg_sz);
    }

    //
    ++svc_ctx->message_count_;

    int reserve_msg = 0;
    if (svc_ctx->profile_)
    {
        svc_ctx->cpu_start_ = time_helper::thread_time();

        // message callback
        reserve_msg = svc_ctx->msg_callback_(svc_ctx, svc_ctx->cb_ud_, svc_msg_type, msg->session_id, msg->src_svc_handle, msg->data_ptr, msg_sz);

        uint64_t cost_time = time_helper::thread_time() - svc_ctx->cpu_start_;
        svc_ctx->cpu_cost_ += cost_time;
    }
    else
    {
        // message callback
        reserve_msg = svc_ctx->msg_callback_(svc_ctx, svc_ctx->cb_ud_, svc_msg_type, msg->session_id, msg->src_svc_handle, msg->data_ptr, msg_sz);
    }

    //
    if (reserve_msg == 0)
    {
        delete[] msg->data_ptr;
    }
}

}


