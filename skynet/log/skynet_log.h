#pragma once

#include <stdio.h>
#include <cstdint>

namespace skynet {

struct skynet_context;

// open logger file
FILE* skynet_log_open(skynet_context* ctx, uint32_t handle);
// close logger file
void skynet_log_close(skynet_context* ctx, FILE* f, uint32_t handle);

// log
void skynet_log_output(FILE* f, uint32_t source, int type, int session, void* buffer, size_t sz);

}

