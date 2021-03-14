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
    // service message queue link list
    mq_private*                     head_ = nullptr;                    // service private message queue head ptr
    mq_private*                     tail_ = nullptr;                    // service private message queue tail ptr

    std::mutex                      mutex_;                             // service message queue link list protect

public:
    // initialize
    void init();

public:
    // push/pop message
    void push(mq_private* q);
    mq_private* pop();
};

}

