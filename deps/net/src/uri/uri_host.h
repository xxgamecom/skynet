#pragma once

#include <string>
#include <regex>

namespace skynet::net {

// uri host part
class uri_host final
{
private:
    std::string value_ = "";

public:
    uri_host() = default;
    explicit uri_host(std::string str)
    :
    value_(std::move(str))
    {
        if (value_ == "*") value_ = "0.0.0.0";
    }

public:
    const std::string& value() const;

public:
    static uri_host from_string(std::string str);
    static std::string to_string(const uri_host& host);
};

}

#include "uri_host.inl"

