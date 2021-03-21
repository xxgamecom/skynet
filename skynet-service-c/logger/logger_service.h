#pragma once

#include "skynet.h"

namespace skynet { namespace service {

/**
 * c service mod: logger
 *
 * this is the first service in skynet node.
 * when skynet node start, the default logger is this service, you can config to use snlua logger.
 *
 * config file param:
 * logger     - lua logger service filename
 * logservice - lua logger service loader (snlua)
 *
 * config file examples:
 * logger = "userlog"
 * logservice = "snlua"
 *
 * log api:
 * skynet::log()
 */
class logger_service : public cservice
{
private:
    FILE*                       log_handle_ = nullptr;          // log handle, it can be a file handle (log to file) or stdout (log to stdout)
    std::string                 log_filename_ = "";             // log file name;
    uint32_t                    start_seconds_ = 0;             // the number of seconds since skynet node started.

    bool                        is_log_to_file_ = false;        // log to file flag
                                                                // true - log to file;
                                                                // false - log to stdout.

public:
    logger_service() = default;
    virtual ~logger_service() = default;

    // cservice impl
public:
    bool init(service_context* svc_ctx, const char* param) override;
    void fini() override;
    void signal(int signal) override;
    int callback(service_context* svc_ctx, int msg_ptype, int session_id, uint32_t src_svc_handle, const void* msg, size_t sz) override;
};

} }

