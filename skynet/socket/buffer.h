#pragma once

#include <cstdlib>

namespace skynet {

// 缓存类型
enum buffer_type
{
    BUFFER_TYPE_MEMORY              = 0,                            // memory
    BUFFER_TYPE_OBJECT              = 1,                            // object
    BUFFER_TYPE_RAW_POINTER         = 2,                            // user data ptr
};

// 发送缓存
struct send_buffer
{
    int                             socket_id = 0;                  // socket id
    int                             type = BUFFER_TYPE_MEMORY;      // buffer type
    const void*                     data_ptr = nullptr;             // data
    size_t                          data_size = 0;                  // data size
};

//
#define USER_OBJECT                 ((size_t)(-1))

// 
struct send_object
{
    const void*                     buffer = nullptr;               //
    size_t                          sz = 0;                         //
    void (*free_func)(void*);
};

}

