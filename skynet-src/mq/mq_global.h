#pragma once

#include <mutex>

namespace skynet {

// forward declare
class mq_private;

// global message queue (link list)
class mq_global final
{
private:
    static mq_global* instance_;
public:
    static mq_global* instance();

private:
    // service mq link list
    mq_private*                     head_ = nullptr;
    mq_private*                     tail_ = nullptr;
    std::mutex                      mutex_;

public:
    // initialize
    void init();

    // push a service private mq to global mq link list
    void push(mq_private* q);
    // pop a service private mq from global mq link list
    mq_private* pop();
};

}

