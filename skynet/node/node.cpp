#include "node.h"
#include "node_thread.h"
#include "node_config.h"
#include "skynet_socket.h"

#include "../mq/mq_msg.h"
#include "../mq/mq_private.h"
#include "../mq/mq_global.h"

#include "../log/log.h"
#include "../log/service_log.h"

#include "../mod/cservice_mod_manager.h"
#include "../timer/timer_manager.h"

#include "../service/service_context.h"
#include "../service/service_monitor.h"
#include "../service/service_manager.h"

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
    skynet_send(NULL, src_svc_handle, msg->src_svc_handle, message_type::PTYPE_ERROR, 0, nullptr, 0);
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
    cservice_mod_manager::instance()->init(config_.cservice_path_);
    //
    timer_manager::instance()->init();
    //
    skynet_socket_init();

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
    skynet_socket_free();

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

// static void copy_name(char name[GLOBALNAME_LENGTH], const char* addr)
// {
//     int i;
//     for (i=0;i<GLOBALNAME_LENGTH && addr[i];i++)
//     {
//         name[i] = addr[i];
//     }
//     for (;i<GLOBALNAME_LENGTH;i++)
//     {
//         name[i] = '\0';
//     }
// }


//
static void _filter_args(service_context* svc_ctx, int type, int* session, void** data, size_t* sz)
{
    int need_copy = !(type & message_type::TAG_DONT_COPY);
    int alloc_session = type & message_type::TAG_ALLOC_SESSION;
    type &= 0xff;

    if (alloc_session)
    {
        assert(*session == 0);
        *session = svc_ctx->new_session();
    }

    if (need_copy && *data)
    {
        char* msg = new char[*sz + 1];
        ::memcpy(msg, *data, *sz);
        msg[*sz] = '\0';
        *data = msg;
    }

    *sz |= (size_t)type << MESSAGE_TYPE_SHIFT;
}

// 发送消息
// ctx之间通过消息进行通信，调用skynet_send向对方发送消息(skynet_sendname最终也会调用skynet_send)。
// @param svc_ctx            源服务的ctx，可以为NULL，drop_message时这个参数为NULL
// @param src_svc_handle         源服务地址，通常设置为0即可，api里会设置成ctx->handle，当context为NULL时，需指定source
// @param dst_svc_handle     目的服务地址
// @param type             消息类型， skynet定义了多种消息，PTYPE_TEXT，PTYPE_CLIENT，PTYPE_RESPONSE等（详情见skynet.h）
// @param session         如果在type里设上allocsession的tag(message_type::TAG_ALLOC_SESSION)，api会忽略掉传入的session参数，重新生成一个新的唯一的
// @param data             消息包数据
// @param sz             消息包长度
// @return int session, 源服务保存这个session，同时约定，目的服务处理完这个消息后，把这个session原样发送回来(skynet_message结构里带有一个session字段)，
//         源服务就知道是哪个请求的返回，从而正确调用对应的回调函数。
int skynet_send(service_context* svc_ctx, uint32_t src_svc_handle, uint32_t dst_svc_handle , int type, int session, void* data, size_t sz)
{
    if ((sz & MESSAGE_TYPE_MASK) != sz)
    {
        log(svc_ctx, "The message to %x is too large", dst_svc_handle);
        if (type & message_type::TAG_DONT_COPY)
        {
//             skynet_free(data);
        }
        return -2;
    }

    // 预处理消息数据块
    _filter_args(svc_ctx, type, &session, (void **)&data, &sz);

    if (src_svc_handle == 0)
    {
        src_svc_handle = svc_ctx->svc_handle_;
    }

    if (dst_svc_handle == 0)
    {
        if (data)
        {
            log(svc_ctx, "Destination address can't be 0");
            // skynet_free(data);
            return -1;
        }

        return session;
    }

    // push message to dst service
    struct skynet_message smsg;
    smsg.src_svc_handle = src_svc_handle;
    smsg.session = session;
    smsg.data = data;
    smsg.sz = sz;
    if (service_manager::instance()->push_service_message(dst_svc_handle, &smsg))
    {
        // skynet_free(data);
        return -1;
    }

    return session;
}

int skynet_send_by_name(service_context* svc_ctx, uint32_t src_svc_handle, const char* dst_name_or_addr, int type, int session, void* data, size_t sz)
{
    if (src_svc_handle == 0)
        src_svc_handle = svc_ctx->svc_handle_;

    uint32_t des = 0;
    // service address
    if (dst_name_or_addr[0] == ':')
    {
        des = ::strtoul(dst_name_or_addr + 1, NULL, 16);
    }
        // local service
    else if (dst_name_or_addr[0] == '.')
    {
        des = service_manager::instance()->find_by_name(dst_name_or_addr + 1);
        if (des == 0)
        {
            if (type & message_type::TAG_DONT_COPY)
            {
                // skynet_free(data);
            }
            return -1;
        }
    }
    else
    {
        if ((sz & MESSAGE_TYPE_MASK) != sz)
        {
            log(svc_ctx, "The message to %s is too large", dst_name_or_addr);
            if (type & message_type::TAG_DONT_COPY)
            {
//                 skynet_free(data);
            }
            return -2;
        }
        _filter_args(svc_ctx, type, &session, (void**)&data, &sz);

//         remote_message* rmsg = skynet_malloc(sizeof(*rmsg));
//         copy_name(rmsg->destination.name, dst_name_or_addr);
//         rmsg->destination.handle = 0;
//         rmsg->message = data;
//         rmsg->sz = sz & MESSAGE_TYPE_MASK;
//         rmsg->type = sz >> MESSAGE_TYPE_SHIFT;

//         skynet_harbor_send(rmsg, src_svc_handle, session);
//         return session;
    }

    return skynet_send(svc_ctx, src_svc_handle, des, type, session, data, sz);
}


}


