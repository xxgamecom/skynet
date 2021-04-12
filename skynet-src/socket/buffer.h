#pragma once

#include "socket_server_def.h"

#include <cstdlib>

namespace skynet {

//----------------------------------------------
// socket send data
//----------------------------------------------

// socket send data type
enum send_data_type
{
    SEND_DATA_TYPE_MEMORY = 0,              // memory data
    SEND_DATA_TYPE_OBJECT = 1,              // object data
    SEND_DATA_TYPE_USER_DATA_PTR = 2,       // user data, must be a raw pointer, can't be a socket object or a memory object.
};

// socket send data, used to send data in lua layer
struct send_data
{
    int socket_id = 0;                      // socket id
    int type = SEND_DATA_TYPE_MEMORY;       // buffer type
    const void* data_ptr = nullptr;         // data
    size_t data_size = 0;                   // data size
};

// use object tag
#define USER_OBJECT_TAG ((size_t)(-1))

// 
struct send_object
{
    const void* buffer = nullptr;           //
    size_t sz = 0;                          //
    void (* free_func)(void*);
};

//----------------------------------------------
// socket send buffer
//----------------------------------------------

// send buffer
struct send_buffer
{
    send_buffer* next = nullptr;                               //

    const void* buffer = nullptr;                               //
    char* ptr = nullptr;                                        //
    size_t sz = 0;                                              //
    bool is_user_object = false;                                //
    uint8_t udp_address[UDP_ADDRESS_SIZE] = { 0 };              //
};

// send buffer list
struct send_buffer_list
{
    send_buffer* head = nullptr;
    send_buffer* tail = nullptr;
};

}

