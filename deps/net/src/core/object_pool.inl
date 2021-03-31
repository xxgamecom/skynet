namespace skynet { namespace net {

template<typename T>
template<typename ...Args>
inline object_pool<T>::object_pool(size_t pool_size, const Args&... args)
:
pool_(),
pool_size_(pool_size)
{
    assert(pool_size_ > 0);
    
    std::shared_ptr<T> obj_ptr;
    for (size_t i=0; i<pool_size_; ++i)
    {
        obj_ptr = std::make_shared<T>(args...);
        if (obj_ptr != nullptr) pool_.push_back(obj_ptr);
    }
}

template<typename T>
inline std::shared_ptr<T> object_pool<T>::alloc()
{
    if (pool_.empty())
        return nullptr;

    std::shared_ptr<T> obj_ptr = pool_.front();
    pool_.pop_front();
    
    return obj_ptr;
}

template<typename T>
inline void object_pool<T>::free(std::shared_ptr<T> obj_ptr)
{
    pool_.push_back(obj_ptr);
}

template<typename T>
inline void object_pool<T>::clear()
{
    pool_.clear();
}

template<typename T>
inline size_t object_pool<T>::pool_size()
{
    return pool_size_;
}

template<typename T>
inline size_t object_pool<T>::free_count()
{
    return pool_.size();
}

template<typename T>
inline size_t object_pool<T>::used_count()
{
    return pool_size_ - pool_.size();
}

} }

