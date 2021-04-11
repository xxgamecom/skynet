#pragma once

#include <mutex>

namespace skynet {

/**
 * 调用单次锁定
 * 用于函数内嵌套调用时锁定资源, 单次实际只锁定一次, 防止出现阻塞
 */
class socket_lock final
{
private:
    std::mutex& mutex_ref_;         // socket mutex reference
    int count_ = 0;                 // lock count

public:
    socket_lock(std::mutex& mutex_ref);

public:
    // 
    void lock();
    //
    bool try_lock();
    //
    void unlock();
};

}
