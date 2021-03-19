#pragma once

namespace skynet { namespace service {

// logger service mod data
struct logger_mod
{
    FILE*                       log_handle = nullptr;           // log handle (file handle | stdout)
    std::string                 log_filename = "";              // log file name;
    uint32_t                    start_seconds = 0;              // the number of seconds since skynet node started.
    int                         close = 0;                      // 是否输出到文件标识, 0代表文件句柄是标准输出, 1代表输出到文件
};

} }

