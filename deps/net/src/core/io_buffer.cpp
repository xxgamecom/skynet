#include "io_buffer.h"

namespace skynet { namespace net {

io_buffer::io_buffer(size_t buf_size/* = BUFFER_SIZE_DEFAULT*/)
:
buf_size_(buf_size)
{
    assert(buf_size_ > 0);

    // alloc memory
    buf_ptr_.reset(new(std::nothrow) char[buf_size_], std::default_delete<char[]>());
    if (buf_ptr_ == nullptr)
    {
        buf_size_ = 0;
    }
}

io_buffer::io_buffer(const char* data_ptr, size_t data_len)
:
buf_size_(data_len)
{
    assert(buf_size_ > 0);

    // alloc memory
    buf_ptr_.reset(new(std::nothrow) char[buf_size_], std::default_delete<char[]>());

    // alloc failed
    if (buf_ptr_ == nullptr)
    {
        buf_size_ = 0;
    }
    // alloc success, copy data
    else
    {
        ::memcpy(buf_ptr_.get(), data_ptr, data_len);
        data_size_ = data_len;
    }
}

} }
