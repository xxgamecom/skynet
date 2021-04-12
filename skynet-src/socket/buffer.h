#pragma once

#include "socket_server_def.h"

#include <cstdlib>

namespace skynet {

//----------------------------------------------
// socket send buffer
//----------------------------------------------

// socket send buffer type
enum buffer_type
{
    BUFFER_TYPE_MEMORY = 0,                 // memory
    BUFFER_TYPE_OBJECT = 1,                 // object
    BUFFER_TYPE_RAW_POINTER = 2,            // user data ptr
};

// socket send buffer
struct send_buffer
{
    int socket_id = 0;                      // socket id
    int type = BUFFER_TYPE_MEMORY;          // buffer type
    const void* data_ptr = nullptr;         // data
    size_t data_size = 0;                   // data size
};

//
#define USER_OBJECT ((size_t)(-1))

// 
struct send_object
{
    const void* buffer = nullptr;           //
    size_t sz = 0;                          //
    void (* free_func)(void*);
};

//----------------------------------------------
// socket write buffer
//----------------------------------------------

// send buffer
struct write_buffer
{
    write_buffer* next = nullptr;                               //

    const void* buffer = nullptr;                               //
    char* ptr = nullptr;                                        //
    size_t sz = 0;                                              //
    bool is_user_object = false;                                //
    uint8_t udp_address[UDP_ADDRESS_SIZE] = { 0 };              //
};

// send buffer list
struct write_buffer_list
{
    write_buffer* head = nullptr;
    write_buffer* tail = nullptr;
};

}

