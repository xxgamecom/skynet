#pragma once

#include "skynet.h"
#include "log_config.h"

namespace skynet { namespace service {

/**
 * c service mod: logger
 *
 * this is the first service in skynet node.
 * when skynet node start, the default logger is this service, you can config to use snlua logger.
 *
 * config file param:
 * log_file    - lua logger service filename
 * log_service - lua logger service loader (snlua)
 *
 * config file examples:
 * log_file = "userlog"
 * log_service = "snlua"
 *
 * log api:
 * skynet::log()
 */
class logger_service : public cservice
{
private:
    log_config log_config_;                 //

private:
    FILE*                       log_handle_ = nullptr;          // log handle, it can be a file handle (log to file) or stdout (log to stdout)
    std::string                 log_filename_ = "";             // log file name;

    bool                        is_log_to_file_ = false;        // log to file flag
                                                                // true - log to file;
                                                                // false - log to stdout.

public:
    logger_service() = default;
    virtual ~logger_service();

    // cservice impl
public:
    bool init(service_context* svc_ctx, const char* param) override;
    void fini() override;
    void signal(int signal) override;

public:
    static int logger_cb(service_context* svc_ctx, void* ud, int svc_msg_type, int session_id, uint32_t src_svc_handle, const void* msg, size_t sz);
};

} }

