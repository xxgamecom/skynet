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
 * log api:
 * skynet::log()
 */
class logger_service : public cservice
{
private:
    log_config log_config_;

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

