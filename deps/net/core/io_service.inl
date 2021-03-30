namespace skynet { namespace network {

inline asio::io_service& io_service::get_raw_ios()
{
    return ios_;
}

inline void io_service::run()
{
//    ios_thread_ptr_ = std::make_shared<std::thread>(std::bind(&asio::io_service::run, &ios_));
    ios_thread_ptr_ = std::make_shared<std::thread>([&](){
        ios_.run();
    });
}

inline void io_service::stop()
{
    ios_.stop();
    if (ios_thread_ptr_ != nullptr)
    {
        ios_thread_ptr_->join();
        ios_thread_ptr_.reset();
    }
}

} }

