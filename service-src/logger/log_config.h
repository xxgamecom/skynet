#pragma once

#include "log_def.h"

#include <string>

namespace skynet {

// log base info
struct log_config_base
{
    uint32_t type_ = LOG_TYPE_CONSOLE;                          // log type
    std::string basename_ = DEFAULT_LOG_BASENAME;               // log filename prefix
    log_level level_ = LOG_LEVEL_INFO;                          // log level, default info
    log_level_type level_type_ = LOG_LEVEL_TYPE_LONG;           // log level_type, default long
};

// log rotating file config
struct log_config_rotating
{
    int max_files_ = DEFAULT_LOG_ROTATING_MAX_FILES;            // the number of log rotating file
    int max_size_ = DEFAULT_LOG_ROTATING_MAX_SIZE;              // the size of log rotating file
};

// log daily file config
struct log_config_daily
{
    int rotating_hour_ = DEFAULT_LOG_DAILY_ROTATING_HOUR;
    int rotation_minute_ = DEFAULT_LOG_DAILY_ROTATING_MINUTE;
};

// log config
class log_config
{
public:
    log_config_base base_;
    log_config_rotating rotating_;
    log_config_daily daily_;
};

}
