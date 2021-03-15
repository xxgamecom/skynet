#pragma once

#include <stdio.h>
#include <cstdint>
#include <atomic>

namespace skynet {

class cservice_mod;
class mq_private;
class service_context;

//
typedef int (*skynet_cb)(service_context* svc_ctx, void* ud, int type, int session, uint32_t source , const void* msg, size_t sz);

/**
 * service_context
 * 
 * 我们创建一个新的服务，首先要先找到对应服务的module，在创建完module实例并完成初始化以后，
 * 还需要创建一个 service_context 上下文，并将module实例和module模块和这个context关联起来，
 * 最后放置于 service_context list中，一个个独立的沙盒环境就这样被创建出来了
 */

// 一个skynet服务ctx的结构
// skynet核心数据结构, 类似于操作系统中的进程的概念, 在这里我们把它称之为服务, 运行和管理服务实例
class service_context
{
public:
    // mod info
    void*                       instance_ = nullptr;        // service mod own data block
    cservice_mod*               mod_ = nullptr;             // 保存module的指针，方便之后调用create,init,signal,release

    // callback
    void*                       cb_ud_;                     // 给callback函数调用第二个参数，可以是NULL, 调用callback函数时，回传给callback的userdata，一般是instance指针
    skynet_cb                   cb_;                        // 消息回调函数指针，通常在module的init里设置

    //
    mq_private*                 queue_ = nullptr;           // ctx自己的消息队列指针

    std::atomic<FILE*>          log_fd_ { nullptr };        // service log fd

    // skynet cmd result
    char                        cmd_result_[32];            // store skynet_command op result

    //
    uint32_t                    svc_handle_ = 0;            // 标识唯一的ctx id, 和进程ID类似, 用于标识唯一服务, 每个服务都关联一个句柄, 句柄的实现在 skynet_handle.h|c 中, 句柄是一个32位无符号整型，最高8位表示集群ID(已不推荐使用)，剩下的24位为服务ID
    int                         session_id_;                // 本方发出请求会设置一个对应的session，当收到对方消息返回时，通过session匹配是哪一个请求的返回
    std::atomic<int>            ref_ { 0 };                 // 引用计数，当为0，可以删除ctx

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
    // 创建服务的一个新的session id
    int new_session();

    // 增加服务引用计数
    void grab();
    //
    void reserve();
    // 
    uint32_t handle();

    // 设置回调函数
    void callback(void* ud, skynet_cb cb);
};

}

#include "service_context.inl"
