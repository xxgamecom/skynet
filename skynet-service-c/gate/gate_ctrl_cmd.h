#pragma once

namespace skynet { namespace service {

// forward declare
struct gate_mod;

//
void handle_ctrl_cmd(gate_mod* mod_ptr, const char* msg, int sz);

} }
