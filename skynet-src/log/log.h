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
void log_debug(service_context* svc_ctx, const char* msg, ...);
void log_info(service_context* svc_ctx, const char* msg, ...);
void log_warn(service_context* svc_ctx, const char* msg, ...);
void log_error(service_context* svc_ctx, const char* msg, ...);

}

