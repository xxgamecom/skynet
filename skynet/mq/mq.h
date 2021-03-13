/**
 * service message queue
 * 
 * 
 * global message qeueu
 * 1) 
 */

#pragma once

#include <stdlib.h>
#include <stdint.h>

#include <mutex>

namespace skynet {

// forward declare
struct skynet_message;

// message drop function
typedef void (*message_drop_proc)(skynet_message* message, void* ud);


//----------------------------------------------
// message_queue
//----------------------------------------------

// service private message queue (one per service)
// memory alginment for performance
class message_queue
{
    //
    // factory
    //

public:
    // factory method, create a service private message queue
    static message_queue* create(uint32_t svc_handle);

    //
    // message_queue
    //

private:
    // constants
    enum
    {
        DEFAULT_QUEUE_CAPACITY      = 64,                                               // default ring buffer size
        DEFAULT_OVERLOAD_THRESHOLD  = 1024,                                             // default overload threshold
    };

public:
    std::mutex                      mutex_;                                             // message ring buffer protect

    uint32_t                        svc_handle_ = 0;                                    // the service handle to which it belongs
    bool                            is_release_ = false;                                // release mark（当delete ctx时会设置此标记）
    bool                            is_in_global_ = true;                               // false: not in global mq;
                                                                                        // true:  in global mq, or the message is dispatching.

    int                             overload_ = 0;                                      // current overload
    int                             overload_threshold_ = DEFAULT_OVERLOAD_THRESHOLD;   // 过载阈值，初始是MQ_OVERLOAD

    int                             cap_ = DEFAULT_QUEUE_CAPACITY;                      // message ring buffer capacity
    int                             head_ = 0;                                          // message ring buffer header
    int                             tail_ = 0;                                          // message ring buffer tailer
    skynet_message*                 queue_ = nullptr;                                   // message ring buffer

    message_queue*                  next_ = nullptr;                                    // 指向下一个消息队列, 链表

public:
    // return the length of message queue, for debug
    int length();
    //
    int overload();    

    void push(skynet_message* message);
    int pop(skynet_message* message);

    // 
    void mark_release();
    void release(message_drop_proc drop_func, void* ud);

private:
    // 扩展循环数组
    void _expand_queue();

    // 释放队列, 释放服务，清空循环数组
    static void _drop_queue(message_queue* q, message_drop_proc drop_func, void* ud);
};

//----------------------------------------------
// global_mq
//----------------------------------------------

// global message queue (link list)
class global_mq final
{
private:
    static global_mq* instance_;
public:
    static global_mq* instance();

private:
    message_queue*                  head_ = nullptr;                    // 头, 指向一个ctx私有队列的头指针
    message_queue*                  tail_ = nullptr;                    // 尾, 指向一个ctx私有队列的尾指针

    std::mutex                      mutex_;                             // 自旋锁, 保证同一时刻只有一个线程在处理

public:
    void init();

    void push(message_queue* q);
    message_queue* pop();
};

}

