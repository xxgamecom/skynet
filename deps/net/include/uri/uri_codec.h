#pragma once

#include "uri_scheme.h"
#include "uri_host.h"
#include "uri_port.h"

#include <sstream>
#include <regex>

namespace skynet { namespace net {

/**
 * uri codec
 * ~~~~~~~~~
 * tcp://www.host.com:1080
 * udp://www.host.com:1080
 * \____/\___________/\__/
 * scheme    host     port
 */
class uri_codec final
{
private:
    uri_scheme scheme_;
    uri_host host_;
    uri_port port_;

public:
    uri_codec() = default;
    uri_codec(uri_scheme scheme, uri_host host, uri_port port)
    :
    scheme_(std::move(scheme)),
    host_(std::move(host)),
    port_(std::move(port))
    {
    }

public:
    const uri_scheme& scheme() const;
    void scheme(uri_scheme scheme);

    const uri_host& host() const;
    void host(uri_host host);

    const uri_port& port() const;
    void port(uri_port port);

    bool is_valid() const;

public:
    static uri_codec from_string(const std::string& str);
    static std::string to_string(const uri_codec& uri);
};

} }

#include "uri_codec.inl"

