namespace skynet::net::impl {

inline asio::io_service& io_service_impl::get_raw_ios()
{
    return ios_;
}

inline void io_service_impl::run()
{
    ios_thread_ptr_ = std::make_shared<std::thread>([&](){
        ios_.run();
    });
}

inline void io_service_impl::stop()
{
    ios_.stop();
    if (ios_thread_ptr_ != nullptr)
    {
        ios_thread_ptr_->join();
        ios_thread_ptr_.reset();
    }
}

}

