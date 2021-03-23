/**
 * builtin log interface
 *
 * in skynet node config file: 
 * - logger = nil, log to stdout;
 * - logger = "./skynet.log", log to logger file.
 */

#pragma once

namespace skynet {

class service_context;

// log to logger service
void log(service_context* svc_ctx, const char* msg, ...);

}

