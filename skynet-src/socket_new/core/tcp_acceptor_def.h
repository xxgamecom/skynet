#pragma once

#include "asio.hpp"

namespace skynet { namespace network {

static constexpr int32_t DEFAULT_BACKLOG = asio::ip::tcp::acceptor::max_connections;

} }

