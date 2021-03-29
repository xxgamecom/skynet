#include "logger_service.h"
#include "spdlog/spdlog.h"
#include "spdlog/sinks/null_sink.h"
#include "spdlog/sinks/stdout_sinks.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/sinks/hourly_file_sink.h"
#include "spdlog/sinks/daily_file_sink.h"
#include "spdlog/sinks/rotating_file_sink.h"

#include <string>
#include <regex>

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

static std::string& _trim(std::string& s)
{
    if (s.empty())
        return s;

    s.erase(0, s.find_first_not_of(" "));
    s.erase(s.find_last_not_of(" ") + 1);
    return s;
}

logger_service::~logger_service()
{
    fini();
}

bool logger_service::init(service_context* svc_ctx, const char* param)
{
    std::string value;

    // type
    value = _get_env(svc_ctx, "logger_type", DEFAULT_LOG_TYPE);
    // split type by ','
    std::regex re {','};
    std::vector<std::string> type_info {
        std::sregex_token_iterator(value.begin(), value.end(), re, -1),
        std::sregex_token_iterator()
    };

    log_config_.base_.type_ = LOG_TYPE_NULL;
    for (auto& type : type_info)
    {
        log_config_.base_.type_ |= string_to_logger_type(_trim(type).c_str());
    }

    // basename
    log_config_.base_.basename_ = _get_env(svc_ctx, "logger_file_basename", DEFAULT_LOG_BASENAME);

    // log level
    value = _get_env(svc_ctx, "logger_log_level", DEFAULT_LOG_LEVEL);
    log_config_.base_.level_ = string_to_log_level(value.c_str());

    //
    std::shared_ptr<spdlog::sinks::sink> file_sink_ptr;
    std::shared_ptr<spdlog::sinks::sink> console_sink_ptr;

    if (log_config_.base_.type_ == LOG_TYPE_NULL)
    {
        file_sink_ptr = std::make_shared<spdlog::sinks::null_sink_st>();
    }
    else
    {
        // console sink
        if ((log_config_.base_.type_ & LOG_TYPE_CONSOLE) != 0)
        {
            console_sink_ptr = std::make_shared<spdlog::sinks::stdout_sink_st>();
        }
        else if ((log_config_.base_.type_ & LOG_TYPE_CONSOLE_COLOR) != 0)
        {
            console_sink_ptr = std::make_shared<spdlog::sinks::stderr_color_sink_st>();
        }

        // file sink
        if ((log_config_.base_.type_ & LOG_TYPE_HOURLY) != 0)
        {
            file_sink_ptr = std::make_shared<spdlog::sinks::hourly_file_sink_st>(log_config_.base_.basename_);
        }
        else if ((log_config_.base_.type_ & LOG_TYPE_DAILY) != 0)
        {
            // read daily log config
            value = _get_env(svc_ctx, "logger_daily_rotating_hour", "");
            if (value.empty())
                log_config_.daily_.rotating_hour_ = DEFAULT_LOG_DAILY_ROTATING_HOUR;
            else
                log_config_.daily_.rotating_hour_ = std::stoi(value);

            value = _get_env(svc_ctx, "logger_daily_rotation_minute", "");
            if (value.empty())
                log_config_.daily_.rotation_minute_ = DEFAULT_LOG_DAILY_ROTATING_MINUTE;
            else
                log_config_.daily_.rotation_minute_ = std::stoi(value);

            //
            file_sink_ptr = std::make_shared<spdlog::sinks::daily_file_sink_st>(log_config_.base_.basename_, log_config_.daily_.rotating_hour_, log_config_.daily_.rotation_minute_);
        }
        else if ((log_config_.base_.type_ & LOG_TYPE_ROTATING) != 0)
        {
            // read rotation log config
            value = _get_env(svc_ctx, "logger_rotating_max_size", "");
            if (value.empty())
                log_config_.rotating_.max_size_= DEFAULT_LOG_ROTATING_MAX_SIZE * 1024 * 1024;
            else
                log_config_.rotating_.max_size_ = std::stoi(value) * 1024 * 1024;

            value = _get_env(svc_ctx, "logger_rotating_max_files", "");
            if (value.empty())
                log_config_.rotating_.max_files_ = DEFAULT_LOG_ROTATING_MAX_FILES;
            else
                log_config_.rotating_.max_files_ = std::stoi(value);

            //
            file_sink_ptr = std::make_shared<spdlog::sinks::rotating_file_sink_st>(log_config_.base_.basename_, log_config_.rotating_.max_size_, log_config_.rotating_.max_files_);
        }
    }

    // logger
    std::shared_ptr<spdlog::logger> logger;
    if (console_sink_ptr != nullptr && file_sink_ptr != nullptr)
    {
        spdlog::sinks_init_list sink_list = { console_sink_ptr, file_sink_ptr };
        logger = std::make_shared<spdlog::logger>("skynet_logger", sink_list.begin(), sink_list.end());
    }
    else if (console_sink_ptr != nullptr)
    {
        logger = std::make_shared<spdlog::logger>("skynet_logger", console_sink_ptr);
    }
    else if (file_sink_ptr != nullptr)
    {
        logger = std::make_shared<spdlog::logger>("skynet_logger", file_sink_ptr);
    }
    logger->set_level(to_spdlog_level(log_config_.base_.level_));
    spdlog::set_default_logger(logger);

    //
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
        break;
    }

    return 0;
}

} }
