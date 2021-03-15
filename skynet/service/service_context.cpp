#include "service_context.h"
#include "service_manager.h"

#include "../node/node.h"

namespace skynet {


// // 创建服务的一个session id
int service_context::new_session()
{
    // 获取session id, session id必须为正数
    // session always be a positive number
    int session = ++session_id_;
    if (session <= 0)
    {
        session_id_ = 1;
        return 1;
    }

    return session;
}

// 
void service_context::reserve()
{
    grab();
    // don't count the context reserved, because skynet abort (the worker threads terminate) only when the total context is 0 .
    // the reserved context will be release at last.
    service_manager::instance()->svc_dec();
}


}
