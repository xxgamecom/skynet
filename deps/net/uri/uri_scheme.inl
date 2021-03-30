#pragma once

namespace skynet { namespace network {

inline const std::string& uri_scheme::value() const
{
    return value_;
}

inline bool uri_scheme::is_splash_splash() const
{
    return is_splash_splash_;
}

inline uri_scheme uri_scheme::from_string(std::string str, std::string* out_ptr/* = nullptr*/)
{
    std::regex r("^(\\w+):(.*)$");
    std::smatch m;

    if (std::regex_match(str, m, r))
    {
        return uri_scheme(m[1], find_splash_splash(m[2], out_ptr));
    }
    else
    {
        return uri_scheme();
    }
}

inline std::string uri_scheme::to_string(const uri_scheme& scheme)
{
    std::stringstream ss;
    ss << scheme.value() << ":" << (scheme.is_splash_splash() ? "//" : "");

    return ss.str();
}

inline bool uri_scheme::find_splash_splash(std::string str, std::string* out_ptr)
{
    std::regex _r("^\\/\\/(.*)$");
    std::smatch _m;

    if (std::regex_match(str, _m, _r))
    {
        if (out_ptr != nullptr) *out_ptr = _m[1];
        return true;
    }
    else
    {
        if (out_ptr != nullptr) *out_ptr = std::move(str);
        return false;
    }
}

} }

