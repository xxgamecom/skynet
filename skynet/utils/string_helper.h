#pragma once

#include <string>

namespace skynet {

class string_helper final
{
public:
    // duplicate string (need release)
    static char* dup(const char* str);

    // format
    template<typename ... Args>
    static std::string format(const std::string& fmt, Args ... args)
    {
        auto size_buf = std::snprintf(nullptr, 0, fmt.c_str(), args ...) + 1;
        std::unique_ptr<char[]> buf(new(std::nothrow) char[size_buf]);

        if (!buf)
            return std::string("");

        std::snprintf(buf.get(), size_buf, fmt.c_str(), args ...);
        return std::string(buf.get(), buf.get() + size_buf - 1);
    }
};

}
