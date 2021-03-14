#pragma once

#include <cstdlib>

namespace skynet { namespace socket {

// 缓存类型
enum buffer_type
{
    MEMORY                          = 0,                            // memory
    OBJECT                          = 1,                            // object
    RAW_POINTER                     = 2,                            // user data ptr
};

// 发送缓存
struct send_buffer
{
    int                             socket_id = 0;                  // socket id
    int                             type = buffer_type::MEMORY;     // buffer type
    const void*                     buffer = nullptr;               // data
    size_t                          sz = 0;                         // data size
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

} }

