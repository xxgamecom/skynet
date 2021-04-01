#include "data_buffer.h"
#include "skynet.h"

#include <cstdlib>
#include <string>
#include <cassert>

namespace skynet::service {

static inline void _return_message(data_buffer* db, message_pool* mp)
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

void messagepool_free(message_pool* pool)
{
    message_pool_list* p = pool->pool;
    while (p != nullptr)
    {
        message_pool_list* tmp = p;
        p = p->next;
        skynet_free(tmp);
    }
    pool->pool = nullptr;
    pool->freelist = nullptr;
}

void data_buffer_read(data_buffer* db, message_pool* mp, char* buffer, int sz)
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

void data_buffer_push(data_buffer* db, message_pool* mp, char* data, int sz)
{
    message* m;
    if (mp->freelist != nullptr)
    {
        m = mp->freelist;
        mp->freelist = m->next;
    }
    else
    {
        message_pool_list* mpl = (message_pool_list*)skynet_malloc(sizeof(*mpl));
        message* temp = mpl->pool;
        int i;
        for (i = 1; i < MESSAGE_POOL_SIZE; i++)
        {
            temp[i].buffer = nullptr;
            temp[i].size = 0;
            temp[i].next = &temp[i + 1];
        }
        temp[MESSAGE_POOL_SIZE - 1].next = nullptr;
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

int data_buffer_readheader(data_buffer* db, message_pool* mp, int header_size)
{
    if (db->header == 0)
    {
        // parser header (2 or 4)
        if (db->size < header_size)
        {
            return -1;
        }
        uint8_t plen[4];
        data_buffer_read(db, mp, (char*)plen, header_size);
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

void data_buffer_reset(data_buffer* db)
{
    db->header = 0;
}

void data_buffer_clear(data_buffer* db, message_pool* mp)
{
    while (db->head)
    {
        _return_message(db, mp);
    }
    memset(db, 0, sizeof(*db));
}

}
