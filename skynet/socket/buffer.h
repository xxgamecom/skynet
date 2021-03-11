#pragma once

#include <stdlib.h>

namespace skynet { namespace socket {

// 缓存类型
enum buffer_type
{
    MEMORY                          = 0,                    // 内存
    OBJECT                          = 1,                    // 对象
    RAW_POINTER                     = 2,                    // 用户数据内存指针
};

// 发送缓存
struct send_buffer
{
    int                             socket_id = 0;          // socket id
    int                             type = 0;               // 缓存类型
    const void*                     buffer = nullptr;       // 缓存
    size_t                          sz = 0;                 // 缓存大小
};

//
#define USER_OBJECT                 ((size_t)(-1))

// 
struct send_object
{
    const void*                     buffer = nullptr;       //
    size_t                          sz = 0;                 //
    void (*free_func)(void*);
};

} }

