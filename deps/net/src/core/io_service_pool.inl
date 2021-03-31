namespace skynet { namespace net { namespace impl {

inline std::shared_ptr<io_service>& io_service_pool_impl::select_one()
{
    std::lock_guard<std::mutex> guard(io_services_mutex_);

    return io_services_[select_index_++ % pool_size_];
}

inline uint32_t io_service_pool_impl::pool_size() const
{
    return pool_size_;
}

} } }

