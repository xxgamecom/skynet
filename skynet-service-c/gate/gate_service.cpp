#include "gate_service.h"
#include "skynet.h"

namespace skynet { namespace service {

bool gate_service::init(service_context* svc_ctx, const char* param)
{
    return false;
}

void gate_service::signal(int signal)
{

}
int gate_service::callback(service_context* svc_ctx, void* ud, int msg_ptype, int session_id, uint32_t src_svc_handle, const void* msg, size_t sz)
{
    return 0;
}

} }

