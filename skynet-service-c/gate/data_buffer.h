#pragma once

#include <cstdlib>
#include <string>
#include <cassert>

#define MESSAGEPOOL 1023

struct message
{
    char* buffer;
    int size;
    message* next;
};

struct databuffer
{
    int header;
    int offset;
    int size;
    message* head;
    message* tail;
};

struct messagepool_list
{
    messagepool_list* next;
    message pool[MESSAGEPOOL];
};

struct messagepool
{
    messagepool_list* pool;
    message* freelist;
};

// use memset init struct 

static void messagepool_free(messagepool* pool)
{
    messagepool_list* p = pool->pool;
    while (p != nullptr)
    {
        messagepool_list* tmp = p;
        p = p->next;
        skynet_free(tmp);
    }
    pool->pool = nullptr;
    pool->freelist = nullptr;
}

static inline void _return_message(databuffer* db, messagepool* mp)
{
    message* m = db->head;
    if (m->next == nullptr)
    {
        assert(db->tail == m);
        db->head = db->tail = nullptr;
    }
    else
    {
        db->head = m->next;
    }
    skynet_free(m->buffer);
    m->buffer = nullptr;
    m->size = 0;
    m->next = mp->freelist;
    mp->freelist = m;
}

static void databuffer_read(databuffer* db, messagepool* mp, char* buffer, int sz)
{
    assert(db->size >= sz);
    db->size -= sz;
    for (;;)
    {
        message* current = db->head;
        int bsz = current->size - db->offset;
        if (bsz > sz)
        {
            ::memcpy(buffer, current->buffer + db->offset, sz);
            db->offset += sz;
            return;
        }
        if (bsz == sz)
        {
            ::memcpy(buffer, current->buffer + db->offset, sz);
            db->offset = 0;
            _return_message(db, mp);
            return;
        }
        else
        {
            ::memcpy(buffer, current->buffer + db->offset, bsz);
            _return_message(db, mp);
            db->offset = 0;
            buffer = buffer + bsz;
            sz -= bsz;
        }
    }
}

static void databuffer_push(databuffer* db, messagepool* mp, char* data, int sz)
{
    message* m;
    if (mp->freelist != nullptr)
    {
        m = mp->freelist;
        mp->freelist = m->next;
    }
    else
    {
        messagepool_list* mpl = (messagepool_list*)skynet_malloc(sizeof(*mpl));
        message* temp = mpl->pool;
        int i;
        for (i = 1; i < MESSAGEPOOL; i++)
        {
            temp[i].buffer = nullptr;
            temp[i].size = 0;
            temp[i].next = &temp[i + 1];
        }
        temp[MESSAGEPOOL - 1].next = nullptr;
        mpl->next = mp->pool;
        mp->pool = mpl;
        m = &temp[0];
        mp->freelist = &temp[1];
    }
    m->buffer = data;
    m->size = sz;
    m->next = nullptr;
    db->size += sz;
    if (db->head == nullptr)
    {
        assert(db->tail == nullptr);
        db->head = db->tail = m;
    }
    else
    {
        db->tail->next = m;
        db->tail = m;
    }
}

static int databuffer_readheader(databuffer* db, messagepool* mp, int header_size)
{
    if (db->header == 0)
    {
        // parser header (2 or 4)
        if (db->size < header_size)
        {
            return -1;
        }
        uint8_t plen[4];
        databuffer_read(db, mp, (char*)plen, header_size);
        // big-endian
        if (header_size == 2)
        {
            db->header = plen[0] << 8 | plen[1];
        }
        else
        {
            db->header = plen[0] << 24 | plen[1] << 16 | plen[2] << 8 | plen[3];
        }
    }
    if (db->size < db->header)
        return -1;

    return db->header;
}

static inline void databuffer_reset(databuffer* db)
{
    db->header = 0;
}

static void databuffer_clear(databuffer* db, messagepool* mp)
{
    while (db->head)
    {
        _return_message(db, mp);
    }
    memset(db, 0, sizeof(*db));
}

