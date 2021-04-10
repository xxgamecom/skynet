#pragma once

#include <sstream>
#include <regex>

namespace skynet::net {

// uri scheme part
class uri_scheme final
{
private:
    std::string value_ = "tcp";
    bool is_splash_splash_ = true;

public:
    uri_scheme() = default;
    explicit uri_scheme(std::string src, bool is_splash_splash = true)
    :
    value_(std::move(src)),
    is_splash_splash_(is_splash_splash)
    {
    }

public:
    const std::string& value() const;
    bool is_splash_splash() const;

public:
    static uri_scheme from_string(std::string str, std::string* out_ptr = nullptr);
    static std::string to_string(const uri_scheme& scheme);

    static bool find_splash_splash(std::string str, std::string* out_ptr);
};

}

#include "uri_scheme.inl"

