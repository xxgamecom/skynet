#pragma once

#include <string>
#include <sstream>
#include <regex>

namespace skynet::net {

// uri port part
class uri_port final
{
private:
    uint16_t value_ = 0;

public:
    uri_port() = default;
    explicit uri_port(uint16_t val)
    :
    value_(val)
    {
    }

public:
    uint16_t value() const;

public:
    static uri_port from_string(const std::string& str);
    static std::string to_string(const uri_port& port);
};

}

#include "uri_port.inl"

