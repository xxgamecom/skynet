#pragma once

#include <cstdint>
#include <atomic>

namespace skynet {

// worker thread moniter
class skynet_monitor
{
public:
    std::atomic<int>                version_ { 0 };                     // 自增计数, 每次处理完一次消息增加1, 用于判定服务是否死循环或长时间占用
    int                             check_version_ = 0;                 // 消息处理计数, 如果version和check_version相同, 有两种情况: 1) 5s期间内工作线程没有消息可处理, 此时, destination为0；2) 处理某条消息时, 耗时超过5s，说明可能死循环，输出错误日志
    uint32_t                        src_svc_handle_ = 0;                // 消息源服务地址, 处理消息前记录
    uint32_t                        dst_svc_handle_ = 0;                // 消息目的服务地址, 处理消息前记录。 不为0时, 说明有消息需要处理

public:
    // trigger monitor, record src/dst service
    void trigger(uint32_t src_svc_handle, uint32_t dst_svc_handle);
    // check worker thread (dead loop or 比较耗时), call in monitor thread
    void check();
};

}

