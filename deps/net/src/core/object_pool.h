#pragma once

#include "asio.hpp"

#include <cassert>
#include <cstdint>
#include <memory>
#include <list>

namespace skynet { namespace net {

/**
 * object pool, fixed length (not thread safe)
 */
template<typename T>
class object_pool : public asio::noncopyable
{
protected:
    std::list<std::shared_ptr<T>> pool_;       // object pool
    size_t pool_size_ = 0;                     // object pool size

public:
    template<typename ...Args>
    explicit object_pool(size_t init_pool_size, const Args& ... args);
    ~object_pool() = default;

public:
    std::shared_ptr<T> alloc();
    void free(std::shared_ptr<T> obj_ptr);

    void clear();

    size_t pool_size();

    size_t free_count();
    size_t used_count();
};

} }

#include "object_pool.inl"

