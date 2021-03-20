#include "hash_id.h"

#include <iostream>

int main()
{
    hash_id hash;

    hash_id_init(&hash, 1200);
    int idx = 0;
    for (int i=0; i<1200; i++)
    {
        idx = hash_id_insert(&hash, i);
        std::cout << "insert : " << idx << std::endl;

        idx = hash_id_lookup(&hash, i);
        std::cout << "lookup : " << idx << std::endl;
    }

    bool is_full = hash_id_full(&hash);
    hash_id_remove(&hash, 123);

    int i = 111;
    idx = hash_id_insert(&hash, i);
    std::cout << "insert : " << idx << std::endl;

    idx = hash_id_lookup(&hash, i);
    std::cout << "lookup : " << idx << std::endl;

    //
    try
    {
        int a = std::stoi("a");
    }
    catch (...)
    {
    }

    return 1;
}
