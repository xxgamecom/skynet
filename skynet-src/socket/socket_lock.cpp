#include "socket_lock.h"

#include <cassert>

namespace skynet {

socket_lock::socket_lock(std::mutex& mutex_ref)
:
mutex_ref_(mutex_ref),
count_(0)
{
}

void socket_lock::lock()
{
    // 之前没有被锁定过
    if (count_ == 0)
        mutex_ref_.lock();

    // 增加锁定次数
    ++count_;
}

bool socket_lock::try_lock()
{
    // 之前没有被锁定过
    if (count_ == 0)
    {
        // 尝试加锁失败
        if (!mutex_ref_.try_lock())
            return false;
    }

    // 增加锁定次数
    ++count_;
    return true;
}

void socket_lock::unlock()
{
    // 减少锁定次数
    --count_;

    // 释放锁
    if (count_ <= 0)
    {
        assert(count_ == 0);
        mutex_ref_.unlock();
    }
}

}
