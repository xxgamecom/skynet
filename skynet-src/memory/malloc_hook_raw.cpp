#ifdef DONT_USE_JEMALLOC

#include "malloc_hook.h"
#include "../skynet.h"

void memory_info_dump(void)
{
    skynet::log(NULL, "No jemalloc");
}

size_t mallctl_int64(const char* name, size_t* new_value)
{
    skynet::log(NULL, "No jemalloc : mallctl_int64 %s.", name);
    return 0;
}

int mallctl_opt(const char* name, int* new_value)
{
    skynet::log(NULL, "No jemalloc : mallctl_opt %s.", name);
    return 0;
}

bool mallctl_bool(const char* name, bool* new_value)
{
    skynet::log(NULL, "No jemalloc : mallctl_bool %s.", name);
    return 0;
}

int mallctl_cmd(const char* name)
{
    skynet::log(NULL, "No jemalloc : mallctl_cmd %s.", name);
    return 0;
}

void* skynet_lalloc(void* ptr, size_t osize, size_t nsize)
{
    if (nsize == 0)
    {
        ::free(ptr);
        return nullptr;
    }

    return ::realloc(ptr, nsize);
}

#endif

