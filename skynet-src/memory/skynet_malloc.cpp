#include "skynet_malloc.h"

void* skynet_lalloc(void* ptr, size_t osize, size_t nsize)
{
    if (nsize == 0)
    {
        ::free(ptr);
        return nullptr;
    }

    return ::realloc(ptr, nsize);
}
