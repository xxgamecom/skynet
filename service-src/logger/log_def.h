#pragma once

#include "spdlog/spdlog.h"

namespace skynet {

// logger type
enum logger_type
{
    LOG_TYPE_NULL = 0,              // log null
    LOG_TYPE_CONSOLE = 1,           // log to console
    LOG_TYPE_CONSOLE_COLOR = 2,     // log to console
    LOG_TYPE_HOURLY = 3,            // log to hourly file
    LOG_TYPE_DAILY = 4,             // log daily file
    LOG_TYPE_ROTATING = 5,          // log to rotating file
};

// log level string
static const char* logger_type_names[] { "null", "console", "console_color", "hourly", "daily", "rotating" };

// convert logger_type to string
inline const char* log_type_to_string(logger_type type)
{
    assert(type >= LOG_TYPE_NULL && type <= LOG_TYPE_ROTATING);
    return logger_type_names[type];
}

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
    LOG_LEVEL_INFO = 0,
    LOG_LEVEL_WARN = 1,
    LOG_LEVEL_ERROR = 2,
    LOG_LEVEL_OFF = 3,
};

// log level string
static const char* log_level_names[] { "info", "warn", "error", "off" };

// convert log level to string
inline const char* log_level_to_string(log_level level)
{
    assert(level >= LOG_LEVEL_INFO && level <= LOG_LEVEL_OFF);
    return log_level_names[level];
}

// convert string to log level
inline log_level string_to_log_level(const char* level)
{
    assert(level != nullptr);

    if (::strcasecmp(level, "info") == 0) return LOG_LEVEL_INFO;
    else if (::strcasecmp(level, "warn") == 0) return LOG_LEVEL_WARN;
    else if (::strcasecmp(level, "error") == 0) return LOG_LEVEL_ERROR;
    else if (::strcasecmp(level, "off") == 0) return LOG_LEVEL_OFF;
    else return LOG_LEVEL_INFO;
}

// convert log_level to spdlog log level
inline spdlog::level::level_enum to_spdlog_level(log_level level)
{
    if (level == LOG_LEVEL_INFO) return spdlog::level::level_enum::info;
    else if (level == LOG_LEVEL_WARN) return spdlog::level::level_enum::warn;
    else if (level == LOG_LEVEL_ERROR) return spdlog::level::level_enum::err;
    else if (level == LOG_LEVEL_OFF) return spdlog::level::level_enum::off;
    else return spdlog::level::level_enum::info;
}

// base info
#define DEFAULT_LOG_TYPE                "rotating"          // default log type
#define DEFAULT_LOG_FILE_EXTENSION      "log"               // default log file extension
#define DEFAULT_LOG_FILE_DIR            "./logs"            // default log file directory
#define DEFAULT_LOG_LEVEL               "info"              // default log level

// log rotating file
#define DEFAULT_LOG_ROTATING_FILE_NUMS  5                   // default the number of rotatiing file
#define DEFAULT_LOG_ROTATING_FILE_SIZE  (50 * 1024 * 1024)  // default the size of rotating file, 50MB

// log daily file

// log hourly file

}

