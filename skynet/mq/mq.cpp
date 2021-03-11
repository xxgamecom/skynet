/**
 * 
 * 设计原理:
 * 1. 各个服务(skynet_context_xxx, ctx)之间是通过消息进行通信的。
 * 2. skynet包含了 '全局消息队列' 和 '次级服务消息队列' 的两级队列。skynet开启了多个 os 工作线程 (通过配置文件的thread参数配置), 
 * 每个线程不断的从全局队列里pop出一个次级服务消息队列, 然后分发次级消息队列里的消息, 分发完后视情况是否push回全局队列, 每个 ctx 有自己的次级服务消息队列。
 * 
 * 全局队列: global_mq
 * 头尾指针分别指向一个次级队列，在skynet启动时初始化全局队列。
 * 
 */

#include "mq.h"
// #include "skynet_handle.h"

#include <cassert>

namespace skynet {

#define MAX_GLOBAL_MQ               0x10000         // 最大长度为max(16bit)+1 = 65536

//----------------------------------------------
// message_queue
//----------------------------------------------

message_queue* message_queue::create(uint32_t svc_handle)
{
    message_queue* q = new message_queue;
    q->svc_handle_ = svc_handle;
    q->cap_ = DEFAULT_QUEUE_CAPACITY;
    q->head_ = 0;
    q->tail_ = 0;

    // When the queue is create (always between service create and service init),
    // set in_global flag to avoid push it to global queue.
    // If the service init success, skynet_context_new will call message_queue->push to push it to global queue.
    // 创建队列时可以发送和接收消息，但还不能被工作线程调度，所以设置成MQ_IN_GLOBAL，保证不会push到全局队列，
    // 当ctx初始化完成再直接调用skynet_globalmq_push到全局队列
    q->is_in_global_ = true;  // in global message queue
    q->is_release_ = false;
    q->overload_ = 0;
    q->overload_threshold_ = DEFAULT_OVERLOAD_THRESHOLD;
    
    q->queue_ = new skynet_message[q->cap_]; // 分配cap个skynet_message大小容量
    q->next_ = nullptr;

    return q;
}

// 获取队列长度，注意数组被循环使用的情况
int message_queue::length()
{
    int head, tail, cap;

    // scope lock
    {
        std::lock_guard<std::mutex> lock(mutex_);

        head = this->head_;
        tail = this->tail_;
        cap = this->cap_;
    }

    // 当还没有循环使用数组的时候
    if (head <= tail)
    {
        return tail - head;
    }

    // 当数组已经被循环使用的时候
    return tail + cap - head;
}

// 获取负载情况
int message_queue::overload()
{
    if (overload_ != 0)
    {
        int overload = overload_;
        overload_ = 0; // reset
        return overload;
    }

    return 0;
}

// 向消息队列里push消息
void message_queue::push(skynet_message* message)
{
    assert(message != nullptr);

    std::lock_guard<std::mutex> lock(mutex_);

    // 入队, 因为是循环数组，越界了要重头开始
    // 存到尾部，然后尾部+1，如果超过容量，则重置为0
    queue_[tail_] = *message;
    if (++tail_ >= cap_)
    {
        tail_ = 0;
    }

    // queue is full, need expand
    if (head_ == tail_)
    {
        _expand_queue();
    }

    // not in global mq, push back
    if (!is_in_global_)
    {
        is_in_global_ = true;
        global_mq::instance()->push(this);
    }
}

// 从私有队列里pop一个消息
int message_queue::pop(skynet_message* message)
{
    int ret = 1;

    std::lock_guard<std::mutex> lock(mutex_);

    // 说明队列不是空的
    if (head_ != tail_)
    {
        // 
        *message = queue_[head_++];     // 注意head++，数据不移动，移动的是游标

        ret = 0;
        int head = head_;
        int tail = tail_;
        int cap = cap_;
        
        // 因为是循环数组，超出边界后要重头开始，所以设为0
        if (head >= cap)
        {
            head_ = head = 0;
        }
        // 如果数组被循环使用了，那么tail < head
        int length = tail - head;
        if (length < 0)
        {
            length += cap;
        }
        // 长度要超过阀值了，扩容一倍，和c++的vector一样的策略
        while (length > overload_threshold_)
        {
            overload_ = length;
            overload_threshold_ *= 2;
        }
    }
    // queue is empty
    else
    {
        // reset overload_threshold when queue is empty
        overload_threshold_ = DEFAULT_OVERLOAD_THRESHOLD;
    }

    if (ret != 0)
    {
        is_in_global_ = false;
    }

    return ret;
}

// 服务释放标记
void message_queue::mark_release()
{
    std::lock_guard<std::mutex> lock(mutex_);

    // marked release
    assert(!is_release_);
    is_release_ = true;

    // not in global message queue
    if (!is_in_global_)
    {
        global_mq::instance()->push(this);
    }
}

// 尝试释放私有队列
void message_queue::release(message_drop_proc drop_func, void* ud)
{
    std::unique_lock<std::mutex> lock(mutex_);

    // has marked release, 说明ctx真正delete了，才能释放掉队列，否则继续push到全局队列，等待下一次调度
    if (is_release_)
    {
        lock.unlock();
        _drop_queue(this, drop_func, ud);
    }
    else
    {
        global_mq::instance()->push(this);
        lock.unlock();
    }
}

// 准备释放队列, 释放服务，清空循环数组
void message_queue::_drop_queue(message_queue* q, message_drop_proc drop_func, void* ud)
{
    skynet_message msg;
    // 先向队列里各个消息的源地址发送特定消息，再释放内存
    while (q->pop(&msg) == 0)
    {
        drop_func(&msg, ud);
    }

    assert(q->next_ == nullptr);

    // 回收内存
    delete[] q->queue_;
    delete q;
}

void message_queue::_expand_queue()
{
    // 新建一个数组
    skynet_message* new_queue = new skynet_message[cap_ * 2];

    // 老数据拷过来
    for (int i = 0; i < cap_; i++)
    {
        new_queue[i] = queue_[(head_ + i) % cap_];
    }
    head_ = 0;      // reset ring buffer head
    tail_ = cap_;   // reset ring buffer tail
    cap_ *= 2;      // double capacity

    // 释放老数组
    delete[] queue_;
    queue_ = new_queue;
}

//----------------------------------------------
// global_mq
//----------------------------------------------


global_mq* global_mq::instance_ = nullptr;

global_mq* global_mq::instance()
{
    static std::once_flag oc;
    std::call_once(oc, [&](){ instance_ = new global_mq; });

    return instance_;
}

// 全局队列初始化
void global_mq::init()
{
}

// // 队列处理流程
// // 1) 调用 message_queue->push 向消息队列压入一个消息
// // 2) 然后，调用skynet_globalmq_push把消息队列链到global_queue尾部
// // 3) 从全局链表弹出一个消息队列，处理队列中的消息，如果队列的消息处理完则不压回全局链表，如果未处理完则重新压入全局链表，等待下一次处理
// // 具体的细节还是要查看skynet_context_message_dispatch这个函数

void global_mq::push(message_queue* q)
{
    std::lock_guard<std::mutex> lock(mutex_);

    assert(q->next_ == nullptr);

    // 链表不为空
    if(tail_ != nullptr)
    {
        tail_->next_ = q;
        tail_ = q;
    }
    // 链表为空
    else
    {
        head_ = tail_ = q;
    }
}

// 取链表中第一个消息队列
// 从全局队列pop一个服务私有消息队列
message_queue* global_mq::pop()
{
    std::lock_guard<std::mutex> lock(mutex_);

    message_queue* mq = head_;
    if (mq != nullptr)
    {
        // 注意这里，队列取出来后，就从链表中删除了
        head_ = mq->next_;
        if (head_ == nullptr)
        {
            assert(mq == tail_);
            tail_ = nullptr;
        }
        mq->next_ = nullptr;
    }

    return mq;
}

}

