#include "game.codec/uri/uri_host.h"

namespace skynet { namespace network {

uri_host::uri_host(std::string str)
:
value_(std::move(str))
{
    if (value_ == "*") value_ = "0.0.0.0";
}

} }
