namespace skynet::net {

inline const std::string& uri_host::value() const
{
    return value_;
}

inline uri_host uri_host::from_string(std::string str)
{
    std::regex r("^(\\w+:\\/\\/)?(.*?)([:\\/].*)?$");
    std::smatch m;

    if (std::regex_match(str, m, r))
    {
        return uri_host(m[2]);
    }
    else
    {
        return uri_host();
    }
}

inline std::string uri_host::to_string(const uri_host& host)
{
    return host.value();
}

}

