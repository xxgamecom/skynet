#pragma once

namespace skynet { namespace service {

// forward declare
class gate_service;

//
void handle_ctrl_cmd(gate_service* gate_svc_ptr, const char* msg, int sz);

} }
