#pragma once

#include <mutex>

namespace skynet {

class mq_private;

// global message queue (link list)
class mq_global final
{
private:
    static mq_global* instance_;
public:
    static mq_global* instance();

private:
    mq_private*                     head_ = nullptr;                    // 头, service private message queue ptr
    mq_private*                     tail_ = nullptr;                    // 尾, service private message queue ptr

    std::mutex                      mutex_;                             // 保证同一时刻只有一个线程在处理

public:
    void init();

    //
public:
    // push/pop message
    void push(mq_private* q);
    mq_private* pop();
};

}

