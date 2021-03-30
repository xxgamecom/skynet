#include "game.codec/uri/uri_codec.h"

namespace skynet { namespace network {

uri_codec::uri_codec(uri_scheme scheme, uri_host host, uri_port port)
:
scheme_(std::move(scheme)),
host_(std::move(host)),
port_(std::move(port))
{
}

} }
