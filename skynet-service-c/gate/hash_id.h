#pragma once

struct hashid_node
{
    int                         id = 0;
    hashid_node*                next = nullptr;
};

struct hashid
{
    int                         hashmod = 0;
    int                         cap = 0;
    int                         count = 0;
    hashid_node*                id = nullptr;
    hashid_node**               hash = nullptr;
};

void hashid_init(hashid* hi, int max);

void hashid_clear(hashid* hi);

int hashid_lookup(hashid* hi, int id);

int hashid_remove(hashid* hi, int id);

int hashid_insert(hashid* hi, int id);

int hashid_full(hashid* hi);

