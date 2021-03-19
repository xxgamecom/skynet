#include "hash_id.h"
#include "skynet.h"

#include <cassert>
#include <cstdlib>
#include <string>

void hashid_init(hashid* hi, int max)
{
    int hashcap = 16;
    while (hashcap < max)
    {
        hashcap *= 2;
    }
    hi->hashmod = hashcap - 1;
    hi->cap = max;
    hi->count = 0;
    hi->id = (hashid_node*)skynet_malloc(max * sizeof(hashid_node));
    for (int i = 0; i < max; i++)
    {
        hi->id[i].id = -1;
        hi->id[i].next = nullptr;
    }
    hi->hash = (hashid_node**)skynet_malloc(hashcap * sizeof(hashid_node*));
    ::memset(hi->hash, 0, hashcap * sizeof(hashid_node*));
}

void hashid_clear(hashid* hi)
{
    skynet_free(hi->id);
    skynet_free(hi->hash);
    hi->id = nullptr;
    hi->hash = nullptr;
    hi->hashmod = 1;
    hi->cap = 0;
    hi->count = 0;
}

int hashid_lookup(hashid* hi, int id)
{
    int h = id & hi->hashmod;
    struct hashid_node* c = hi->hash[h];
    while (c)
    {
        if (c->id == id)
            return c - hi->id;
        c = c->next;
    }
    return -1;
}

int hashid_remove(hashid* hi, int id)
{
    int h = id & hi->hashmod;
    struct hashid_node* c = hi->hash[h];
    if (c == nullptr)
        return -1;
    if (c->id == id)
    {
        hi->hash[h] = c->next;
        goto _clear;
    }
    while (c->next)
    {
        if (c->next->id == id)
        {
            struct hashid_node* temp = c->next;
            c->next = temp->next;
            c = temp;
            goto _clear;
        }
        c = c->next;
    }
    return -1;
_clear:
    c->id = -1;
    c->next = nullptr;
    --hi->count;
    return c - hi->id;
}

int hashid_insert(hashid* hi, int id)
{
    hashid_node* c = nullptr;
    for (int i = 0; i < hi->cap; i++)
    {
        int index = (i + id) % hi->cap;
        if (hi->id[index].id == -1)
        {
            c = &hi->id[index];
            break;
        }
    }
    assert(c);
    ++hi->count;
    c->id = id;
    assert(c->next == nullptr);
    int h = id & hi->hashmod;
    if (hi->hash[h])
    {
        c->next = hi->hash[h];
    }
    hi->hash[h] = c;

    return c - hi->id;
}

int hashid_full(hashid* hi)
{
    return hi->count == hi->cap;
}

