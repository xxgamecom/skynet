#include "skynet.h"
#include "gate_ctrl_cmd.h"
#include "gate_service.h"

#include <string>
#include <regex>

namespace skynet::service {

static bool _handle_ctrl_cmd_start(gate_service* gate_svc_ptr, std::vector<std::string>& param_info)
{
    // check param num
    if (param_info.size() < 1)
    {
        log_error(gate_svc_ptr->svc_ctx_, "[gate] start failed, the number of param must == 1");
        return false;
    }

    int socket_id = 0;
    try
    {
        socket_id = std::stoi(param_info[0]);
    }
    catch (...)
    {
        log_error(gate_svc_ptr->svc_ctx_, "[gate] start failed, invalid socket id");
        return false;
    }

    int id = hash_id_lookup(&gate_svc_ptr->hash_, socket_id);
    if (id >= 0)
    {
        node_socket::instance()->start(gate_svc_ptr->svc_ctx_, socket_id);
    }

    return true;
}

static bool _handle_ctrl_cmd_close(gate_service* gate_svc_ptr)
{
    if (gate_svc_ptr->listen_id_ >= 0)
    {
        node_socket::instance()->close(gate_svc_ptr->svc_ctx_, gate_svc_ptr->listen_id_);
        gate_svc_ptr->listen_id_ = -1;
    }

    return true;
}

static bool _handle_ctrl_cmd_kick(gate_service* gate_svc_ptr, std::vector<std::string>& param_info)
{
    // check param num
    if (param_info.size() < 1)
    {
        log_warn(gate_svc_ptr->svc_ctx_, "[gate] kick failed, the number of param must == 1");
        return false;
    }

    int socket_id = 0;
    try
    {
        socket_id = std::stoi(param_info[0]);
    }
    catch (...)
    {
        log_warn(gate_svc_ptr->svc_ctx_, "[gate] kick failed, invalid socket id");
        return false;
    }

    int id = hash_id_lookup(&gate_svc_ptr->hash_, socket_id);
    if (id >= 0)
    {
        node_socket::instance()->close(gate_svc_ptr->svc_ctx_, socket_id);
    }

    return true;
}

static bool _handle_ctrl_cmd_forward(gate_service* gate_svc_ptr, std::vector<std::string>& param_info)
{
    // check param num
    if (param_info.size() < 3)
    {
        log_warn(gate_svc_ptr->svc_ctx_, "[gate] forward failed, the number of param must == 3");
        return false;
    }

    int socket_id = 0;
    try
    {
        socket_id = std::stoi(param_info[0]);
    }
    catch (...)
    {
        log_warn(gate_svc_ptr->svc_ctx_, "[gate] forward failed, invalid socket id");
        return false;
    }

    uint32_t agent_svc_handle = 0;
    try
    {
        // ":0000000A", skip ':'
        agent_svc_handle = std::stoi(param_info[1].substr(1), nullptr, 16);
    }
    catch (...)
    {
        log_warn(gate_svc_ptr->svc_ctx_, "[gate] forward failed, invalid agent service handle");
        return false;
    }

    uint32_t client_svc_handle = 0;
    try
    {
        // ":0000000A", skip ':'
        client_svc_handle = std::stoi(param_info[2].substr(1), nullptr, 16);
    }
    catch (...)
    {
        log_warn(gate_svc_ptr->svc_ctx_, "[gate] forward failed, invalid client service handle");
        return false;
    }

    // forward agent
    int idx = hash_id_lookup(&gate_svc_ptr->hash_, socket_id);
    if (idx >= 0)
    {
        auto agent = gate_svc_ptr->connections_.get()[idx];
        agent.agent_svc_handle = agent_svc_handle;
        agent.client_svc_handle = client_svc_handle;
    }

    return true;
}

static bool _handle_ctrl_cmd_broker(gate_service* gate_svc_ptr, std::vector<std::string>& param_info)
{
    // check param num
    if (param_info.size() < 1)
    {
        log_error(gate_svc_ptr->svc_ctx_, "[gate] forward failed, the number of param must == 1");
        return false;
    }

    std::string name_or_addr = param_info[0];
    gate_svc_ptr->broker_svc_handle_ = service_manager::instance()->query_by_name(gate_svc_ptr->svc_ctx_, name_or_addr.c_str());

    return true;
}


void handle_ctrl_cmd(gate_service* gate_svc_ptr, const char* msg, int sz)
{
    if (sz == 0)
        return;

    std::string cmd_string = msg;

    // split command and args by ' '
    std::regex re {' '};
    std::vector<std::string> cmd_info {
        std::sregex_token_iterator(cmd_string.begin(), cmd_string.end(), re, -1),
        std::sregex_token_iterator()
    };

    // cmd
    std::string cmd = cmd_info[0];

    // param vector
    cmd_info.erase(cmd_info.begin());
    auto& param_info = cmd_info;

    // cmd - "kick socket_id"
    if (cmd == "kick")
    {
        if (!_handle_ctrl_cmd_kick(gate_svc_ptr, param_info))
        {
            log_error(gate_svc_ptr->svc_ctx_, fmt::format("[gate] command : {}", cmd_string));
        }
    }
    // cmd - "forward socket_id agent_svc_handle client_svc_handle"
    else if (cmd == "forward")
    {
        if (!_handle_ctrl_cmd_forward(gate_svc_ptr, param_info))
        {
            log_error(gate_svc_ptr->svc_ctx_, fmt::format("[gate] command : {}", cmd_string));
        }
    }
    // cmd - "broker name_or_addr"
    else if (cmd == "broker")
    {
        if (!_handle_ctrl_cmd_broker(gate_svc_ptr, param_info))
        {
            log_error(gate_svc_ptr->svc_ctx_, fmt::format("[gate] command : {}", cmd_string));
        }
    }
    // cmd - "start socket_id"
    else if (cmd == "start")
    {
        if (!_handle_ctrl_cmd_start(gate_svc_ptr, param_info))
        {
            log_error(gate_svc_ptr->svc_ctx_, fmt::format("[gate] command : {}", cmd_string));
        }
    }
    // cmd - "close"
    else if (cmd == "close")
    {
        _handle_ctrl_cmd_close(gate_svc_ptr);
    }
    else
    {
        log_error(gate_svc_ptr->svc_ctx_, fmt::format("[gate] Unknown command : {}", cmd_string));
    }
}

}

