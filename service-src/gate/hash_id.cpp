#include "hash_id.h"
#include "skynet.h"

#include <cassert>
#include <cstdlib>
#include <string>

namespace skynet::service {

void hash_id_init(hash_id* hi, int max)
{
    int hash_cap = 16;
    while (hash_cap < max)
        hash_cap *= 2;

    hi->hashmod = hash_cap - 1;
    hi->cap = max;
    hi->count = 0;
    hi->hash_id_nodes = (hash_id_node*)skynet_malloc(max * sizeof(hash_id_node));
    for (int i = 0; i < max; i++)
    {
        hi->hash_id_nodes[i].id = -1;
        hi->hash_id_nodes[i].next = nullptr;
    }
    hi->hash = (hash_id_node**)skynet_malloc(hash_cap * sizeof(hash_id_node*));
    ::memset(hi->hash, 0, hash_cap * sizeof(hash_id_node*));
}

void hash_id_clear(hash_id* hi)
{
    skynet_free(hi->hash_id_nodes);
    skynet_free(hi->hash);
    hi->hash_id_nodes = nullptr;
    hi->hash = nullptr;
    hi->hashmod = 1;
    hi->cap = 0;
    hi->count = 0;
}

int hash_id_lookup(hash_id* hi, int id)
{
    int h = id & hi->hashmod;
    hash_id_node* c = hi->hash[h];
    while (c != nullptr)
    {
        if (c->id == id)
            return c - hi->hash_id_nodes;
        c = c->next;
    }
    return -1;
}

int hash_id_remove(hash_id* hi, int id)
{
    int h = id & hi->hashmod;
    hash_id_node* c = hi->hash[h];
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
            hash_id_node* temp = c->next;
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
    return c - hi->hash_id_nodes;
}

int hash_id_insert(hash_id* hi, int id)
{
    hash_id_node* c = nullptr;
    for (int i = 0; i < hi->cap; i++)
    {
        int index = (i + id) % hi->cap;
        if (hi->hash_id_nodes[index].id == -1)
        {
            c = &hi->hash_id_nodes[index];
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

    return c - hi->hash_id_nodes;
}

int hash_id_full(hash_id* hi)
{
    return hi->count == hi->cap;
}

}
