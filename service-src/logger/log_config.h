#pragma once

#include "log_def.h"

#include <string>

namespace skynet {

// log base info
struct log_config_base
{
    logger_type type_ = LOG_TYPE_CONSOLE;                       // log type
    std::string basename_ = "";                                 // log filename prefix
    std::string extension_ = DEFAULT_LOG_FILE_EXTENSION;        // log file extension
    std::string file_dir_ = DEFAULT_LOG_FILE_DIR;               // log file path
    log_level level_ = LOG_LEVEL_INFO;                          // log level, default info
};

// log rotating file config
struct log_config_rotating
{
    int file_nums_ = DEFAULT_LOG_ROTATING_FILE_NUMS;            // the number of log rotating file
    int file_size_ = DEFAULT_LOG_ROTATING_FILE_SIZE;            // the size of log rotating file
};

// log daily file config
struct log_config_daily
{
    int rotating_hour_ = 23;
    int rotation_minute_ = 59;
};

// log hourly file config
struct log_config_hourly
{

};


// log config
class log_config
{
public:
    log_config_base base_;
    log_config_rotating rotating_;
    log_config_daily daily_;
    log_config_hourly hourly_;
};

}
