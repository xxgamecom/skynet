#include "logger_service.h"
#include "spdlog/spdlog.h"
#include "spdlog/sinks/null_sink.h"
#include "spdlog/sinks/stdout_sinks.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/sinks/hourly_file_sink.h"
#include "spdlog/sinks/daily_file_sink.h"
#include "spdlog/sinks/rotating_file_sink.h"

#include <string>

namespace skynet { namespace service {

static const char* _get_env(service_context* svc_ctx, const char* key, const char* default_value)
{
    const char* ret = service_command::exec(svc_ctx, "GET_ENV", key);
    if (ret == nullptr)
    {
        return default_value;
    }

    return ret;
}

logger_service::~logger_service()
{
    fini();
}

bool logger_service::init(service_context* svc_ctx, const char* param)
{
    std::string value;

    // base info
    value = _get_env(svc_ctx, "logger_type", DEFAULT_LOG_TYPE);
    log_config_.base_.type_ = string_to_logger_type(value.c_str());
    log_config_.base_.basename_ = _get_env(svc_ctx, "logger_file_basename", "skynet");
    log_config_.base_.extension_ = _get_env(svc_ctx, "logger_file_extension", DEFAULT_LOG_FILE_EXTENSION);
    log_config_.base_.file_dir_ = _get_env(svc_ctx, "logger_file_dir", DEFAULT_LOG_FILE_DIR);
    value = _get_env(svc_ctx, "logger_log_level", DEFAULT_LOG_LEVEL);
    log_config_.base_.level_ = string_to_log_level(value.c_str());

    std::shared_ptr<spdlog::sinks::sink> sink_ptr;

    switch (log_config_.base_.type_)
    {
    case LOG_TYPE_NULL:
        sink_ptr = std::make_shared<spdlog::sinks::null_sink_st>();
        break;
    case LOG_TYPE_CONSOLE:
        sink_ptr = std::make_shared<spdlog::sinks::stdout_sink_st>();
        break;
    case LOG_TYPE_CONSOLE_COLOR:
        sink_ptr = std::make_shared<spdlog::sinks::stderr_color_sink_st>();
        break;
    case LOG_TYPE_HOURLY:
        sink_ptr = std::make_shared<spdlog::sinks::hourly_file_sink_st>(log_config_.base_.basename_);
        break;
    case LOG_TYPE_DAILY:
        // read daily log config
        value = _get_env(svc_ctx, "logger_daily_rotating_hour", "23");
        log_config_.daily_.rotating_hour_ = std::stoi(value);
        value = _get_env(svc_ctx, "logger_daily_rotation_minute", "59");
        log_config_.daily_.rotation_minute_ = std::stoi(value);

        //
        sink_ptr = std::make_shared<spdlog::sinks::daily_file_sink_st>(log_config_.base_.basename_, log_config_.daily_.rotating_hour_, log_config_.daily_.rotation_minute_);
        break;
    case LOG_TYPE_ROTATING:
        // read rotation log config
        value = _get_env(svc_ctx, "logger_rotating_max_size", "50"); // DEFAULT_LOG_ROTATING_FILE_SIZE
        log_config_.rotating_.file_size_ = std::stoi(value) * 1024 * 1024;
        value = _get_env(svc_ctx, "logger_rotating_max_files", "5"); // DEFAULT_LOG_ROTATING_FILE_NUMS
        log_config_.rotating_.file_nums_ = std::stoi(value);

        //
        sink_ptr = std::make_shared<spdlog::sinks::rotating_file_sink_st>(log_config_.base_.basename_, log_config_.rotating_.file_size_, log_config_.rotating_.file_nums_);
        break;
    default:
        break;
    }

    auto logger = std::make_shared<spdlog::logger>("skynet_logger", sink_ptr);
    logger->set_level(to_spdlog_level(log_config_.base_.level_));
    spdlog::set_default_logger(logger);

    svc_ctx->set_callback(logger_cb, this);
    return true;
}

void logger_service::fini()
{
}

void logger_service::signal(int signal)
{
}

int logger_service::logger_cb(service_context* svc_ctx, void* ud, int svc_msg_type, int session_id, uint32_t src_svc_handle, const void* msg, size_t sz)
{
    auto svc_ptr = (logger_service*)ud;

    switch (svc_msg_type)
    {
    case SERVICE_MSG_TYPE_SYSTEM:
//        if (!svc_ptr->log_filename_.empty())
//        {
//            svc_ptr->log_handle_ = ::freopen(svc_ptr->log_filename_.c_str(), "a", svc_ptr->log_handle_);
//        }
        break;
    case SERVICE_MSG_TYPE_TEXT:
        spdlog::info("[:{:08X}] {}", src_svc_handle, std::string((const char*)msg, sz));
//        spdlog::info("{0:08x} {}", src_svc_handle, s);
//        ::fprintf(svc_ptr->log_handle_, "[:%08x] ", src_svc_handle);
//        ::fwrite(msg, sz, 1, svc_ptr->log_handle_);
//        ::fprintf(svc_ptr->log_handle_, "\n");
//        ::fflush(svc_ptr->log_handle_);
        break;
    }

    return 0;
}

} }
