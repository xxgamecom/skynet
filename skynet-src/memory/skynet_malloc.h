#pragma once

#include <cstdlib>

#ifdef DONT_USE_JEMALLOC

#define skynet_malloc           malloc
#define skynet_calloc           calloc
#define skynet_realloc          realloc
#define skynet_free             free
#define skynet_memalign         memalign
#define skynet_aligned_alloc    aligned_alloc
#define skynet_posix_memalign   posix_memalign

#else

void* skynet_malloc(size_t sz);
void* skynet_calloc(size_t nmemb, size_t size);
void* skynet_realloc(void* ptr, size_t size);
void skynet_free(void* ptr);
void* skynet_memalign(size_t alignment, size_t size);
void* skynet_aligned_alloc(size_t alignment, size_t size);
int skynet_posix_memalign(void** memptr, size_t alignment, size_t size);

#endif

// use for lua memory alloc, 返回值用于lua_newstate的第一个参数
void* skynet_lalloc(void* ptr, size_t osize, size_t nsize);

