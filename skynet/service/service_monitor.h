#pragma once

#include <cstdint>
#include <atomic>

namespace skynet {

// service worker thread monitor
class service_monitor final
{
private:
    std::atomic<int>                last_version_ { 0 };                // self increasing, process_begin() and process_end() will increasing this variable. used to detected dead lock or blocked service.
    int                             check_version_ = 0;                 // 消息处理计数, 如果version和check_version相同, 有两种情况: 1) 5s期间内工作线程没有消息可处理, 此时, destination为0；2) 处理某条消息时, 耗时超过5s，说明可能死循环，输出错误日志

    uint32_t                        src_svc_handle_ = 0;                // message source service handle, record by process_begin()
    uint32_t                        dst_svc_handle_ = 0;                // message destination service handle, record by process_begin(). !=0 means有消息需要处理

public:
    // handle service message begin
    void process_begin(uint32_t src_svc_handle, uint32_t dst_svc_handle);
    // handle service message end
    void process_end();

    // check worker thread (dead loop or blocked), call in monitor thread
    void check();
};

}
