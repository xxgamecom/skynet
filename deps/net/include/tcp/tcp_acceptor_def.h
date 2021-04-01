#pragma once

#include "asio.hpp"

namespace skynet::net {

static constexpr int32_t DEFAULT_BACKLOG = asio::ip::tcp::acceptor::max_connections;

}

