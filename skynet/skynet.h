/**
 * skynet API
 *
 * for c service & luaclib
 */

#pragma once

// memory api:
// skynet_debug_memory()
// skynet_malloc()
// skynet_free()
#include "memory/skynet_malloc.h"

// message api:
// enum message_type
#include "mq/mq_msg.h"

// log api:
// log()
#include "log/log.h"

// service api:
// service_manager::instance()->query_by_name();
// service_manager::instance()->send();
// service_manager::instance()->send_by_name();
// service_manager::instance()->current_handle();
// service_command::handle_command();
// service_context->set_callback();
#include "service/service_context.h"
#include "service/service_manager.h"
#include "service/service_command.h"

// socket api
// node_socket::instance()->
#include "node/node_socket.h"

// time api:
// timer_manager::instance()->now();
#include "timer/timer_manager.h"

// utils api
// time_helper
#include "utils/time_helper.h"

// mod api
// cservice_mod interface
#include "mod/cservice_mod_i.h"

