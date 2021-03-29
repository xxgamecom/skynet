#pragma once

#include "spdlog/spdlog.h"

namespace skynet {

// logger type
enum logger_type
{
    LOG_TYPE_NULL = 0,                  // log null

    LOG_TYPE_CONSOLE = 0x01,            // log to console
    LOG_TYPE_CONSOLE_COLOR = 0x02,      // log to console

    LOG_TYPE_HOURLY = 0x10,             // log to hourly file
    LOG_TYPE_DAILY = 0x20,              // log daily file
    LOG_TYPE_ROTATING = 0x40,           // log to rotating file
};

// convert string to logger_type
inline logger_type string_to_logger_type(const char* type)
{
    assert(type != nullptr);

    if (::strcasecmp(type, "null") == 0) return LOG_TYPE_NULL;
    else if (::strcasecmp(type, "console") == 0) return LOG_TYPE_CONSOLE;
    else if (::strcasecmp(type, "console_color") == 0) return LOG_TYPE_CONSOLE_COLOR;
    else if (::strcasecmp(type, "hourly") == 0) return LOG_TYPE_HOURLY;
    else if (::strcasecmp(type, "daily") == 0) return LOG_TYPE_DAILY;
    else if (::strcasecmp(type, "rotating") == 0) return LOG_TYPE_ROTATING;
    else return LOG_TYPE_CONSOLE;
}


// log level
enum log_level
{
    LOG_LEVEL_DEBUG = 0,
    LOG_LEVEL_INFO = 1,
    LOG_LEVEL_WARN = 2,
    LOG_LEVEL_ERROR = 3,
    LOG_LEVEL_OFF = 4,
};

// convert string to log level
inline log_level string_to_log_level(const char* level)
{
    assert(level != nullptr);

    if (::strcasecmp(level, "debug") == 0) return LOG_LEVEL_DEBUG;
    else if (::strcasecmp(level, "info") == 0) return LOG_LEVEL_INFO;
    else if (::strcasecmp(level, "warn") == 0) return LOG_LEVEL_WARN;
    else if (::strcasecmp(level, "error") == 0) return LOG_LEVEL_ERROR;
    else if (::strcasecmp(level, "off") == 0) return LOG_LEVEL_OFF;
    else return LOG_LEVEL_INFO;
}

// convert log_level to spdlog log level
inline spdlog::level::level_enum to_spdlog_level(log_level level)
{
    if (level == LOG_LEVEL_DEBUG) return spdlog::level::level_enum::debug;
    else if (level == LOG_LEVEL_INFO) return spdlog::level::level_enum::info;
    else if (level == LOG_LEVEL_WARN) return spdlog::level::level_enum::warn;
    else if (level == LOG_LEVEL_ERROR) return spdlog::level::level_enum::err;
    else if (level == LOG_LEVEL_OFF) return spdlog::level::level_enum::off;
    else return spdlog::level::level_enum::info;
}

// base info
#define DEFAULT_LOG_TYPE                    "console"           // default log type
#define DEFAULT_LOG_BASENAME                "./logs/skynet.log" // default log basename
#define DEFAULT_LOG_LEVEL                   "info"              // default log level

// log rotating file
#define DEFAULT_LOG_ROTATING_MAX_FILES      5                   // default the number of rotatiing file
#define DEFAULT_LOG_ROTATING_MAX_SIZE       50                  // default the size of rotating file, 50MB

// log daily file
#define DEFAULT_LOG_DAILY_ROTATING_HOUR     23                  //
#define DEFAULT_LOG_DAILY_ROTATING_MINUTE   59                  //

}

