/**
 * 队列处理流程
 * 1) 调用 mq_private->push 向消息队列压入一个消息
 * 2) 然后，调用 mq_global::push 把消息队列链到 global_queue 尾部
 * 3) 从全局链表弹出一个消息队列，处理队列中的消息，如果队列的消息处理完则不压回全局链表，如果未处理完则重新压入全局链表，等待下一次处理
 * 具体的细节还是要查看 dispatch_message 这个函数
 */

#include "mq_global.h"
#include "mq_private.h"

namespace skynet {

mq_global* mq_global::instance_ = nullptr;

mq_global* mq_global::instance()
{
    static std::once_flag oc;
    std::call_once(oc, [&](){ instance_ = new mq_global; });

    return instance_;
}

// 全局队列初始化
void mq_global::init()
{
}

void mq_global::push(mq_private* q)
{
    std::lock_guard<std::mutex> lock(mutex_);

    assert(q->next_ == nullptr);

    // like list not empty
    if(tail_ != nullptr)
    {
        tail_->next_ = q;
        tail_ = q;
    }
    else
    {
        head_ = tail_ = q;
    }
}

mq_private* mq_global::pop()
{
    std::lock_guard<std::mutex> lock(mutex_);

    mq_private* mq = head_;
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


