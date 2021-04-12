/**
 * service private message queue
 */

#pragma once

#include <stdlib.h>
#include <stdint.h>

#include <mutex>

namespace skynet {

// forward declare
struct service_message;

// message drop function
typedef void (*message_drop_proc)(service_message* message, void* ud);

/**
 * service private message queue (one per service)
 * memory alginment for performance
 */
class mq_private
{
private:
    // constants
    enum
    {
        DEFAULT_QUEUE_CAPACITY = 64,                        // default ring buffer size
        DEFAULT_OVERLOAD_THRESHOLD = 1024,                  // default overload threshold
    };

public:
    std::mutex mutex_;                                      // message ring buffer protect

    uint32_t svc_handle_ = 0;                               // the service handle to which it belongs
    bool is_release_ = false;                               // release mark（当delete ctx时会设置此标记）
    bool is_in_global_ = true;                              // false: not in global mq; true: in global mq, or the message is dispatching.

    int overload_ = 0;                                      // current overload
    int overload_threshold_ = DEFAULT_OVERLOAD_THRESHOLD;   // 过载阈值，初始是MQ_OVERLOAD

    int cap_ = DEFAULT_QUEUE_CAPACITY;                      // message ring buffer capacity
    int head_ = 0;                                          // message ring buffer header
    int tail_ = 0;                                          // message ring buffer tailer
    service_message* queue_ = nullptr;                      // message ring buffer

    mq_private* next_ = nullptr;                            // link list: next message queue ptr

public:
    // factory method, create a service private message queue
    static mq_private* create(uint32_t svc_handle);
    // 服务释放标记
    void mark_release();
    // 尝试释放私有队列
    void release(message_drop_proc drop_func, void* ud);

public:
    // 0 for success
    void push(service_message* message);
    bool pop(service_message* message);

    // return the length of message queue, for debug
    int length();
    // 获取负载情况
    int overload();

    uint32_t svc_handle();

private:
    // 扩展循环数组
    void _expand_queue();

    // 释放队列, 释放服务，清空循环数组
    static void _drop_queue(mq_private* q, message_drop_proc drop_func, void* ud);
};

}

