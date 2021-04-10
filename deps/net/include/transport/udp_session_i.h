#pragma once

#include "asio.hpp"
#include "base/session_i.h"

namespace skynet::net {

class udp_session : public basic_session
{
public:
    virtual ~udp_session() = default;

public:
};

}
