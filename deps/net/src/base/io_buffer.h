#pragma once

#include <cstdlib>
#include <cstring>
#include <cassert>
#include <memory>

namespace skynet::net {

// socket io buffer
class io_buffer
{
private:
    std::shared_ptr<char> buf_ptr_;         // buffer
    size_t buf_size_ = 0;                   // buffer size
    size_t data_size_ = 0;                  // actually data size

public:
    explicit io_buffer(size_t buf_size);
    // build with external buffer, memcpy will be triggered.
    io_buffer(const char* data_ptr, size_t data_len);
    ~io_buffer() = default;

public:
    // get/set data
    char* data();
    void data(const char* data_ptr, size_t data_len);

    // get/set data size
    size_t data_size();
    void data_size(size_t size);

    // buffer capacity
    size_t capacity();

    // clear buffer
    void clear();
};

}

#include "io_buffer.inl"

