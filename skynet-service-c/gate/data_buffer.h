#pragma once

#define MESSAGE_POOL_SIZE 1023

struct message
{
    char*                       buffer = nullptr;
    int                         size = 0;
    message*                    next = nullptr;
};

struct data_buffer
{
    int                         header = 0;
    int                         offset = 0;
    int                         size = 0;
    message*                    head = nullptr;
    message*                    tail = nullptr;
};

struct message_pool_list
{
    message_pool_list*          next = nullptr;
    message                     pool[MESSAGE_POOL_SIZE];
};

struct message_pool
{
    message_pool_list*          pool = nullptr;
    message*                    freelist = nullptr;
};

void messagepool_free(message_pool* pool);

void data_buffer_read(data_buffer* db, message_pool* mp, char* buffer, int sz);

void data_buffer_push(data_buffer* db, message_pool* mp, char* data, int sz);

int data_buffer_readheader(data_buffer* db, message_pool* mp, int header_size);

void data_buffer_reset(data_buffer* db);

void data_buffer_clear(data_buffer* db, message_pool* mp);

