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
    SEND_DATA_TYPE_MEMORY = 0,              // memory data (manage memory by self)

    SEND_DATA_TYPE_USER_OBJECT = 1,         // user object data (manage memory by self)

    SEND_DATA_TYPE_USER_DATA = 2,           // user data(LUA_TUSERDATA, full userdata),
                                            // must be a raw pointer, can't be a socket object or a memory object.
};

// socket send data, used to send data in lua layer
struct send_data
{
    int socket_id = 0;                      // socket logic id
    int type = SEND_DATA_TYPE_MEMORY;       // buffer type, default: memory data
    const void* data_ptr = nullptr;         // data ptr
    size_t data_size = 0;                   // data size
};

// use_object tag
#define USER_OBJECT_TAG ((size_t)(-1))

// socket user_object interface
struct socket_user_object
{
    // get user_object buffer
    const void* (*buffer)(const void* uo_ptr);
    // get user_object size
    size_t (*size)(const void* uo_ptr);
    // free user_object
    void (*free)(void* uo_ptr);
};

// socket send user object
struct send_user_object
{
    const void* buffer = nullptr;           // user_object ptr
    size_t sz = 0;                          // user_object size
    void (*free_func)(void* uo_ptr);        // free user_object function
};


//----------------------------------------------
// socket write buffer
//----------------------------------------------

// write buffer
struct write_buffer
{
    write_buffer* next = nullptr;                               //

    const void* buffer = nullptr;                               //
    char* ptr = nullptr;                                        //
    size_t sz = 0;                                              //
    bool is_user_object = false;                                //
    uint8_t udp_address[UDP_ADDRESS_SIZE] = { 0 };              //
};

// write buffer list
struct write_buffer_list
{
    write_buffer* head = nullptr;
    write_buffer* tail = nullptr;
};

}

