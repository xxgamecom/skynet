// use jemalloc
#ifndef DONT_USE_JEMALLOC

#include "malloc_hook.h"
#include "../skynet.h"

#include "jemalloc.h"

// turn on MEMORY_CHECK can do more memory check, such as double free
//#define MEMORY_CHECK

//
struct mem_cookie
{
    uint32_t                handle;
#ifdef MEMORY_CHECK
    uint32_t                dog_tag;
#endif
};

#define PREFIX_SIZE         sizeof(struct mem_cookie)

// dog tag
#define MEMORY_ALLOC_TAG    0x20140605
#define MEMORY_FREE_TAG     0x0badf00d

static std::atomic<ssize_t>* get_allocated_field(uint32_t handle)
{
    int h = (int) (handle & (SLOT_SIZE - 1));
    struct mem_data* data = &mem_stats[h];
    uint32_t old_handle = data->handle;
    ssize_t old_alloc = data->allocated;
    if (old_handle == 0 || old_alloc <= 0)
    {
        // data->allocated may less than zero, because it may not count at start.
        if (!data->handle.compare_exchange_strong(old_handle, handle))
        {
            return 0;
        }
        if (old_alloc < 0)
        {
            data->allocated.compare_exchange_strong(old_alloc, 0);
        }
    }
    if (data->handle != handle)
    {
        return 0;
    }

    return &data->allocated;
}

inline static void update_xmalloc_stat_alloc(uint32_t handle, size_t __n)
{
    _used_memory += __n;
    ++_memory_block;

    std::atomic<ssize_t>* allocated = get_allocated_field(handle);
    if (allocated != nullptr)
    {
        allocated += __n;
    }
}

inline static void update_xmalloc_stat_free(uint32_t handle, size_t __n)
{
    _used_memory -= __n;
    --_memory_block;

    std::atomic<ssize_t>* allocated = get_allocated_field(handle);
    if (allocated != nullptr)
    {
        allocated -= __n;
    }
}

inline static void* fill_prefix(char* ptr)
{
//    uint32_t handle = skynet_current_handle();
    uint32_t handle;
    size_t size = je_malloc_usable_size(ptr);
    struct mem_cookie* p = (struct mem_cookie*) (ptr + size - sizeof(struct mem_cookie));
    ::memcpy(&p->handle, &handle, sizeof(handle));
#ifdef MEMORY_CHECK
    uint32_t dog_tag = MEMORY_ALLOC_TAG;
    ::memcpy(&p->dog_tag, &dog_tag, sizeof(dog_tag));
#endif
    update_xmalloc_stat_alloc(handle, size);
    return ptr;
}

inline static void* clean_prefix(char* ptr)
{
    size_t size = je_malloc_usable_size(ptr);
    struct mem_cookie* p = (struct mem_cookie*) (ptr + size - sizeof(struct mem_cookie));
    uint32_t handle;
    ::memcpy(&handle, &p->handle, sizeof(handle));
#ifdef MEMORY_CHECK
    uint32_t dog_tag;
    ::memcpy(&dog_tag, &p->dog_tag, sizeof(dog_tag));
    if (dog_tag == MEMORY_FREE_TAG)
    {
        ::fprintf(stderr, "xmalloc: double free in :%08x\n", handle);
    }
    // memory out of bounds
    assert(dog_tag == MEMORY_ALLOC_TAG);
    dog_tag = MEMORY_FREE_TAG;
    ::memcpy(&p->dog_tag, &dog_tag, sizeof(dog_tag));
#endif
    update_xmalloc_stat_free(handle, size);

    return ptr;
}

static void malloc_oom(size_t size)
{
    ::fprintf(stderr, "xmalloc: Out of memory trying to allocate %zu bytes\n", size);
    ::fflush(stderr);
    ::abort();
}

void memory_info_dump(void)
{
    je_malloc_stats_print(0, 0, 0);
}

bool mallctl_bool(const char* name, bool* new_value)
{
    bool v = 0;
    size_t len = sizeof(v);
    if (new_value != nullptr)
    {
        je_mallctl(name, &v, &len, new_value, sizeof(bool));
    }
    else
    {
        je_mallctl(name, &v, &len, NULL, 0);
    }
    return v;
}

int mallctl_cmd(const char* name)
{
    return je_mallctl(name, NULL, NULL, NULL, 0);
}

size_t mallctl_int64(const char* name, size_t* new_value)
{
    size_t v = 0;
    size_t len = sizeof(v);
    if (new_value != nullptr)
    {
        je_mallctl(name, &v, &len, new_value, sizeof(size_t));
    }
    else
    {
        je_mallctl(name, &v, &len, NULL, 0);
    }
    // skynet::log(NULL, "name: %s, value: %zd\n", name, v);
    return v;
}

int mallctl_opt(const char* name, int* new_value)
{
    int v = 0;
    size_t len = sizeof(v);
    if (new_value)
    {
        int ret = je_mallctl(name, &v, &len, new_value, sizeof(int));
        if (ret == 0)
        {
            skynet::log(NULL, "set new value(%d) for (%s) succeed\n", *new_value, name);
        }
        else
        {
            skynet::log(NULL, "set new value(%d) for (%s) failed: error -> %d\n", *new_value, name, ret);
        }
    }
    else
    {
        je_mallctl(name, &v, &len, NULL, 0);
    }

    return v;
}

//
// hook : malloc, realloc, free, calloc
//

void* skynet_malloc(size_t size)
{
    void* ptr = je_malloc(size + PREFIX_SIZE);
    if (ptr == nullptr)
        malloc_oom(size);
    return fill_prefix(ptr);
}

void* skynet_realloc(void* ptr, size_t size)
{
    if (ptr == nullptr)
        return skynet_malloc(size);

    void* rawptr = clean_prefix(ptr);
    void* newptr = je_realloc(rawptr, size + PREFIX_SIZE);
    if (newptr == nullptr)
        malloc_oom(size);

    return fill_prefix(newptr);
}

void skynet_free(void* ptr)
{
    if (ptr == nullptr)
        return;

    void* rawptr = clean_prefix(ptr);
    je_free(rawptr);
}

void* skynet_calloc(size_t nmemb, size_t size)
{
    void* ptr = je_calloc(nmemb + ((PREFIX_SIZE + size - 1) / size), size);
    if (ptr == nullptr)
        malloc_oom(size);

    return fill_prefix(ptr);
}

void* skynet_memalign(size_t alignment, size_t size)
{
    void* ptr = je_memalign(alignment, size + PREFIX_SIZE);
    if (ptr == nullptr)
        malloc_oom(size);

    return fill_prefix(ptr);
}

void* skynet_aligned_alloc(size_t alignment, size_t size)
{
    void* ptr = je_aligned_alloc(alignment, size + (size_t)((PREFIX_SIZE + alignment - 1) & ~(alignment - 1)));
    if (ptr == nullptr)
        malloc_oom(size);
    return fill_prefix(ptr);
}

int skynet_posix_memalign(void** memptr, size_t alignment, size_t size)
{
    int err = je_posix_memalign(memptr, alignment, size + PREFIX_SIZE);
    if (err)
        malloc_oom(size);
    fill_prefix(*memptr);
    return err;
}

void* skynet_lalloc(void* ptr, size_t osize, size_t nsize)
{
    if (nsize == 0)
    {
        je_free(ptr);
        return nullptr;
    }

    return je_realloc(ptr, nsize);
}

#endif

