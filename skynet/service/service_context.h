#pragma once

#include <stdio.h>
#include <cstdint>
#include <atomic>

namespace skynet {

// forward declare
struct service_mod_info;
class mq_private;
class service_context;

namespace service {
class cservice;
}

//
typedef int (*skynet_cb)(service_context* svc_ctx, void* ud, int type, int session, uint32_t source , const void* msg, size_t sz);

/**
 * service_context
 *
 * 还需要创建一个 service_context 上下文，并将module实例和module模块和这个context关联起来，
 * 最后放置于 service_context list中，一个个独立的沙盒环境就这样被创建出来了
 */
class service_context
{
public:
    // c service mod
    service_mod_info*           svc_mod_ptr_ = nullptr;     // c service mod
    service::cservice*          svc_ptr_ = nullptr;         // c service instance

    // callback
    void*                       cb_ud_ = nullptr;           // callback function argument, 调用callback函数时, 回传给callback的 user data, 一般是instance指针
    skynet_cb                   cb_ = nullptr;              // callback function

    //
    mq_private*                 queue_ = nullptr;           // service private queue

    std::atomic<FILE*>          log_fd_ { nullptr };        // service log fd

    char                        cmd_result_[32] = { 0 };    // store service_command op result

    //
    uint32_t                    svc_handle_ = 0;            // 标识唯一的ctx id, 和进程ID类似, 用于标识唯一服务, 每个服务都关联一个句柄, 句柄的实现在 skynet_handle.h|c 中, 句柄是一个32位无符号整型，最高8位表示集群ID(已不推荐使用)，剩下的24位为服务ID
    int                         session_id_ = 0;            // 本方发出请求会设置一个对应的session，当收到对方消息返回时，通过session匹配是哪一个请求的返回
    std::atomic<int>            ref_ { 0 };                 // ref count, if ==0, can delete this ctx

    bool                        is_init_ = false;           // service initialize tag
    bool                        is_blocked_ = false;        // service blocked tag
                                                            // set by service monitor thread when service dead lock or blocked.

    // stat
    int                         message_count_ = 0;         // 累计收到的消息数量

    // cpu usage
    uint64_t                    cpu_cost_ = 0;              // in microsec
    uint64_t                    cpu_start_ = 0;             // in microsec
    bool                        profile_ = false;           // 标记是否需要开启性能监测(记录cpu调用时间)

public:
    // new session id
    int new_session();

    //
    void grab();

    void set_callback(void* ud, skynet_cb cb);
};

}

#include "service_context.inl"
