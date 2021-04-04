#pragma once

namespace skynet::service {

struct hash_id_node
{
    int id = 0;
    hash_id_node* next = nullptr;
};

struct hash_id
{
    int hashmod = 0;
    int cap = 0;
    int count = 0;
    hash_id_node* hash_id_nodes = nullptr;
    hash_id_node** hash = nullptr;
};

void hash_id_init(hash_id* hi, int max);

void hash_id_clear(hash_id* hi);

int hash_id_lookup(hash_id* hi, int id);

int hash_id_remove(hash_id* hi, int id);

int hash_id_insert(hash_id* hi, int id);

int hash_id_full(hash_id* hi);

}
