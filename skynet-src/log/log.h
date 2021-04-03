/**
 * builtin log interface
 */

#pragma once

#include "fmt/format.h"

namespace skynet {

class service_context;

void log_debug(service_context* svc_ctx, const char* msg);
void log_info(service_context* svc_ctx, const char* msg);
void log_warn(service_context* svc_ctx, const char* msg);
void log_error(service_context* svc_ctx, const char* msg);

void log_debug(service_context* svc_ctx, const std::string& msg);
void log_info(service_context* svc_ctx, const std::string& msg);
void log_warn(service_context* svc_ctx, const std::string& msg);
void log_error(service_context* svc_ctx, const std::string& msg);


}

