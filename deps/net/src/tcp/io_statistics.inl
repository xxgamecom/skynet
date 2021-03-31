namespace skynet { namespace net { namespace impl {

// 更新吞吐量计数器
inline void io_statistics::update_throughput()
{
    int64_t interval = 0;
    {
        std::lock_guard<std::mutex> lock(throughput_update_mutex_);

        // 计算是否需要更新吞吐计数
        interval = timeout_count_ * UPDATE_READ_WRITE_INTERVAL_SECONDS;
        if (interval < THROUGHPUT_INTERVAL_SECONDS) return;
    }

    // 重置超时次数
    timeout_count_ = 0;

    // 更新吞吐量
    int64_t read_bytes = read_bytes_.load(std::memory_order_relaxed);
    int64_t write_bytes = write_bytes_.load(std::memory_order_relaxed);

    {
        std::lock_guard<std::mutex> lock(throughput_update_mutex_);

        read_bytes_throughput_ = static_cast<double>(read_bytes  - last_read_bytes_) / interval;
        write_bytes_throughput_ = static_cast<double>(write_bytes - last_write_bytes_) / interval;

        // 更新最大吞吐量
        if (read_bytes_throughput_ > largest_read_bytes_throughput_)
            largest_read_bytes_throughput_ = read_bytes_throughput_;
        if (write_bytes_throughput_ > largest_write_bytes_throughput_)
            largest_write_bytes_throughput_ = write_bytes_throughput_;
    }

    //fmt::MemoryWriter w;
    //w.write("read: {}, last_read:  {}, read_throughput:  {:.2f}, largest_read_throughput:  {:.2f}",
    //        read_bytes,
    //        last_read_bytes_.load(std::memory_order_relaxed),
    //        read_bytes_throughput_,
    //        largest_read_bytes_throughput_);
    //std::cout << w.c_str() << std::endl;
    //w.clear();
    //w.write("write: {}, last_write: {}, write_throughput: {:.2f}, largest_write_throughput: {:.2f}",
    //        write_bytes,
    //        last_write_bytes_.load(std::memory_order_relaxed),
    //        write_bytes_throughput_,
    //        largest_write_bytes_throughput_);
    //std::cout << w.c_str() << std::endl;

    // 更新最近读写计数
    last_read_bytes_.store(read_bytes, std::memory_order_relaxed);
    last_write_bytes_.store(write_bytes, std::memory_order_relaxed);
}

// 读字节数
inline int64_t io_statistics::read_bytes()
{
    return read_bytes_.load(std::memory_order_relaxed);
}

// 写字节数
inline int64_t io_statistics::write_bytes()
{
    return write_bytes_.load(std::memory_order_relaxed);
}


// 读字节数吞吐量(每秒字节数)
inline double io_statistics::read_bytes_throughput()
{
    std::lock_guard<std::mutex> lock(throughput_update_mutex_);

    return read_bytes_throughput_;
}

// 写字节数吞吐量(每秒字节数)
inline double io_statistics::write_bytes_throughput()
{
    std::lock_guard<std::mutex> lock(throughput_update_mutex_);

    return write_bytes_throughput_;
}

// 最大读字节数吞吐量(每秒读字节数)
inline double io_statistics::largest_read_bytes_throughput()
{
    std::lock_guard<std::mutex> lock(throughput_update_mutex_);

    return largest_read_bytes_throughput_;
}

// 最大写字节数吞吐量(每秒写字节数)
inline double io_statistics::largest_write_bytes_throughput()
{
    std::lock_guard<std::mutex> lock(throughput_update_mutex_);

    return largest_write_bytes_throughput_;
}

} } }
