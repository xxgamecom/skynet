namespace skynet { namespace net {

inline void uri_codec::scheme(uri_scheme scheme)
{
    scheme_ = std::move(scheme);
}

inline const uri_scheme& uri_codec::scheme() const
{
    return scheme_;
}

inline void uri_codec::host(uri_host host)
{
    host_ = std::move(host);
}

inline const uri_host& uri_codec::host() const
{
    return host_;
}

inline void uri_codec::port(uri_port port)
{
    port_ = std::move(port);
}

inline const uri_port& uri_codec::port() const
{
    return port_;
}

inline bool uri_codec::is_valid() const
{
    return (!host_.value().empty() &&
            !scheme_.value().empty() &&
            port_.value() != 0);
}

inline uri_codec uri_codec::from_string(const std::string& str)
{
    uri_scheme scheme = uri_scheme::from_string(str);
    uri_host host = uri_host::from_string(str);
    uri_port port = uri_port::from_string(str);

    return uri_codec(scheme, host, port);
}

inline std::string uri_codec::to_string(const uri_codec& uri)
{
    std::stringstream ss;
    ss << uri_scheme::to_string(uri.scheme_)
       << uri_host::to_string(uri.host_)
       << uri_port::to_string(uri.port_);

    return ss.str();
}

} }

