#pragma once

#include <stdio.h>
#include <cstdint>
#include <atomic>

namespace skynet {

class cservice_mod;
class message_queue;

typedef int (*skynet_cb)(struct skynet_context * context, void *ud, int type, int session, uint32_t source , const void * msg, size_t sz);

// /**
//  * skynet_context管理模块
//  * 
//  * 我们创建一个新的服务，首先要先找到对应服务的module，在创建完module实例并完成初始化以后，
//  * 还需要创建一个skynet_context上下文，并将module实例和module模块和这个context关联起来，
//  * 最后放置于skynet_context list中，一个个独立的沙盒环境就这样被创建出来了
//  */

// 一个skynet服务ctx的结构
// skynet核心数据结构, 类似于操作系统中的进程的概念, 在这里我们把它称之为服务, 运行和管理服务实例
struct skynet_context
{
public:
    // mod info
    void*                       instance_ = nullptr;        // 每个ctx自己的数据块，不同类型ctx有不同数据结构，相同类型ctx数据结构相同，但具体的数据不一样，由指定module的create函数返回
    cservice_mod*               mod_ = nullptr;             // 保存module的指针，方便之后调用create,init,signal,release

    // callback
    void*                       cb_ud_;                     // 给callback函数调用第二个参数，可以是NULL, 调用callback函数时，回传给callback的userdata，一般是instance指针
    skynet_cb                   cb_;                        // 消息回调函数指针，通常在module的init里设置

    //
    message_queue*              queue_;                     // ctx自己的消息队列指针

    //
    std::atomic<FILE*>          log_fd_ { nullptr };        // 

    // skynet cmd result
    char                        result_[32];                // 保存skynet_command操作后的结果

    //
    uint32_t                    svc_handle_ = 0;            // 标识唯一的ctx id, 和进程ID类似, 用于标识唯一服务, 每个服务都关联一个句柄, 句柄的实现在 skynet_handle.h|c 中, 句柄是一个32位无符号整型，最高8位表示集群ID(已不推荐使用)，剩下的24位为服务ID
    int                         session_id_;                // 本方发出请求会设置一个对应的session，当收到对方消息返回时，通过session匹配是哪一个请求的返回
    std::atomic<int>            ref_ { 0 };                 // 引用计数，当为0，可以删除ctx

    bool                        init_;                      // 标记是否完成初始化
    bool                        endless_;                   // 标记消息是否堵住 (monitor线程监控服务是否超过规定时间, 如果规定时间内服务没有执行完成, 则会设置该标记)

    // stat
    int                         message_count_;             // 累计收到的消息数量

    // cpu usage
    uint64_t                    cpu_cost_;                  // in microsec
    uint64_t                    cpu_start_;                 // in microsec
    bool                        profile_;                   // 标记是否需要开启性能监测(记录cpu调用时间)

//     // 定义调用自旋锁
//     CHECKCALLING_DECL

public:
    // 创建服务的一个新的session id
    int newsession();
    //
    void grab();
    //
    void reserve();
    // 
    uint32_t handle();

    //
    void callback(void* ud, skynet_cb cb);
};

// 创建一个服务：name为服务模块的名字，parm为参数，由模块自己解释含义
// @param name 模块名称, skynet根据这个名字加载模块, 并调用约定好的导出函数
skynet_context* skynet_context_new(const char * name, const char * parm);
// 创建服务ctx
skynet_context* skynet_context_release(skynet_context* ctx);

}

