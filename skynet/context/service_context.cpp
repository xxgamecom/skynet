#include "service_context.h"
#include "service_context_manager.h"

#include "../node/node.h"

#include "../log/log.h"

#include "../mq/mq_msg.h"
#include "../mq/mq_private.h"
#include "../mq/mq_global.h"

#include "../mod/cservice_mod_manager.h"

namespace skynet {

// 
static void delete_context(service_context* svc_ctx)
{
    if (svc_ctx->log_fd_ != nullptr)
    {
        ::fclose(svc_ctx->log_fd_);
    }

    svc_ctx->mod_->instance_release(svc_ctx->instance_);
    svc_ctx->queue_->mark_release();

    delete svc_ctx;
    node::instance()->dec_svc_ctx();
}

// // 创建服务的一个session id
int service_context::new_session()
{
    // 获取session id, session id必须为正数
    // session always be a positive number
    int session = ++session_id_;
    if (session <= 0)
    {
        session_id_ = 1;
        return 1;
    }

    return session;
}

// 
void service_context::reserve()
{
    grab();
    // don't count the context reserved, because skynet abort (the worker threads terminate) only when the total context is 0 .
    // the reserved context will be release at last.
    node::instance()->dec_svc_ctx();
}


// 启动一个新服务ctx：name为服务模块的名字，parm为参数，由模块自己解释含义
// @param svc_name 服务模块名
// @param param 服务参数
service_context* service_context_new(const char* svc_name, const char* param)
{
    // query c service
    cservice_mod* mod = cservice_mod_manager::instance()->query(svc_name);
    if (mod == nullptr)
        return nullptr;

    // create service mod own data block (如: struct snlua, struct logger,  struct gate)
    void* inst = mod->instance_create();
    if (inst == nullptr)
        return nullptr;
    
    // create service context
    service_context* svc_ctx = new service_context;

//     CHECKCALLING_INIT(svc_ctx)

    svc_ctx->mod_ = mod;
    svc_ctx->instance_ = inst;
    svc_ctx->ref_ = 2;        // 初始化完成会调用 service_context_release 将引用计数-1，ref变成1而不会被释放掉
    svc_ctx->cb_ = nullptr;
    svc_ctx->cb_ud_ = nullptr;
    svc_ctx->session_id_ = 0;
    svc_ctx->log_fd_ = nullptr;

    svc_ctx->is_init_ = false;
    svc_ctx->is_blocked_ = false;

    svc_ctx->cpu_cost_ = 0;
    svc_ctx->cpu_start_ = 0;
    svc_ctx->message_count_ = 0;
    svc_ctx->profile_ = node::instance()->is_profile();

    // Should set to 0 first to avoid service_context_manager::retire_all() get an uninitialized handle
    // initialize function maybe use svc_ctx->svc_handle_, so it must init at last
    svc_ctx->svc_handle_ = 0;
    svc_ctx->svc_handle_ = service_context_manager::instance()->register_svc_ctx(svc_ctx); // register service context and get service handle

    // initialize service private queue
    mq_private* queue = mq_private::create(svc_ctx->svc_handle_);
    svc_ctx->queue_ = queue;

    // increase service count
    node::instance()->inc_svc_ctx();

    // init mod data
    CHECKCALLING_BEGIN(svc_ctx)
    int r = mod->instance_init(inst, svc_ctx, param);
    CHECKCALLING_END(svc_ctx)

    // service mod initialize success
    if (r == 0)
    {
        service_context* ret = service_context_release(svc_ctx);
        if (ret != nullptr)
            svc_ctx->is_init_ = true;

        // put service private queue into global mq. service can recv message now.
        mq_global::instance()->push(queue);
        if (ret != nullptr)
            log(ret, "LAUNCH %s %s", svc_name, param ? param : "");

        return ret;
    } 
    // service mod initialize failed
    else
    {
        log(svc_ctx, "FAILED launch %s", svc_name);

        uint32_t svc_handle = svc_ctx->svc_handle_;
        service_context_release(svc_ctx);
        service_context_manager::instance()->retire(svc_handle);
        // drop_t d = { svc_handle };
        // queue->release(drop_message, &d);

        return nullptr;
    }
}

// 创建服务ctx
service_context* service_context_release(service_context* svc_ctx)
{
    if (--svc_ctx->ref_ == 0)
    {
        delete_context(svc_ctx);
        return nullptr;
    }

    return svc_ctx;
}

// 投递服务消息
int service_context_push(uint32_t svc_handle, skynet_message* message)
{
    // 增加服务引用计数
    service_context* svc_ctx = service_context_manager::instance()->grab(svc_handle);
    if (svc_ctx == nullptr)
        return -1;

    // 消息入队
    svc_ctx->queue_->push(message);
    // 减少服务引用计数
    // service_context_release(svc_ctx);

    return 0;
}

// 发送消息
void service_context_send(service_context* svc_ctx, void* msg, size_t sz, uint32_t src_svc_handle, int type, int session)
{
    skynet_message smsg;
    smsg.src_svc_handle = src_svc_handle;
    smsg.session = session;
    smsg.data = msg;
    smsg.sz = sz | (size_t)type << MESSAGE_TYPE_SHIFT;

    svc_ctx->queue_->push(&smsg);
}

void service_context_blocked(uint32_t svc_handle)
{
    service_context* svc_ctx = service_context_manager::instance()->grab(svc_handle);
    if (svc_ctx == nullptr)
        return;

    // mark blocked
    svc_ctx->is_blocked_ = true;
    // service_context_release(svc_ctx);
}

}
