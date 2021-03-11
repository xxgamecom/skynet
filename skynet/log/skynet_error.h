#pragma once

namespace skynet {

struct skynet_context;

//
void skynet_error(skynet_context* context, const char* msg, ...);

}

