namespace skynet { namespace net {

inline char* io_buffer::data()
{
    return buf_ptr_.get();
}

inline void io_buffer::data(const char* data_ptr, size_t data_len)
{
    assert(data_len <= buf_size_);

    ::memcpy(buf_ptr_.get(), data_ptr, data_len);
    data_size_ = data_len;
}

inline size_t io_buffer::data_size()
{
    return data_size_;
}

inline void io_buffer::data_size(size_t size)
{
    data_size_ = size;
}

inline size_t io_buffer::capacity()
{
    return buf_size_;
}

inline void io_buffer::clear()
{
    data_size_ = 0;
}

} }

