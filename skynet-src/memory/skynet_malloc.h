#pragma once

#include <cstdlib>

#define skynet_malloc           malloc
#define skynet_calloc           calloc
#define skynet_realloc          realloc
#define skynet_free             free
#define skynet_memalign         memalign
#define skynet_aligned_alloc    aligned_alloc
#define skynet_posix_memalign   posix_memalign

// use for lua memory alloc, 返回值用于lua_newstate的第一个参数
void* skynet_lalloc(void* ptr, size_t osize, size_t nsize);

