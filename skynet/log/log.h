/**
 * builtin log interface
 *
 * in skynet node config file: 
 * - logger = nil, log to stdout;
 * - logger = "./skynet.log", log to logger file.
 */

#pragma once

namespace skynet {

struct skynet_context;

// log to logger service
void log(skynet_context* context, const char* msg, ...);

}

