#include "game.codec/uri/uri_scheme.h"

namespace skynet { namespace network {

uri_scheme::uri_scheme(std::string str, bool is_splash_splash/* = true*/)
:
value_(std::move(str)),
is_splash_splash_(is_splash_splash)
{
}

} }
