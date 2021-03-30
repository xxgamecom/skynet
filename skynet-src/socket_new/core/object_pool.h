#pragma once

#include <cstdint>
#include <list>

namespace skynet { namespace network {

// object pool, fixed length (not thread safe)
template<typename T>
class object_pool
{
protected:
    std::list<std::shared_ptr<T>> pool_;       // object pool
    size_t pool_size_ = 0;                     // object pool size

public:
    template<typename ...Args>
    explicit object_pool(size_t init_pool_size, const Args& ... args);
    ~object_pool() = default;

public:
    // 分配/释放
    std::shared_ptr<T> alloc();
    void free(std::shared_ptr<T> obj_ptr);

    // 清理
    void clear();

    // 获取池对象大小
    size_t pool_size();
    // 获取空闲数量
    size_t free_count();
    // 获取使用数量
    size_t used_count();

    // noncopyable
private:
    object_pool(const object_pool&) = delete;
    object_pool& operator=(const object_pool&) = delete;
};

} }

#include "object_pool.inl"

