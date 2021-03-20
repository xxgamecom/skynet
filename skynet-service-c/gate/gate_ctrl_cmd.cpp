#include "skynet.h"
#include "gate_ctrl_cmd.h"
#include "gate_mod.h"

#include <string>
#include <regex>

namespace skynet { namespace service {

static bool _handle_ctrl_cmd_kick(gate_mod* mod_ptr, std::vector<std::string>& param_info)
{
    // check param num
    if (param_info.size() < 1)
    {
        log(mod_ptr->svc_ctx, "[gate] kick failed, the number of param must == 1");
        return false;
    }

    int socket_id = 0;
    try
    {
        socket_id = std::stoi(param_info[0]);
    }
    catch (...)
    {
        log(mod_ptr->svc_ctx, "[gate] kick failed, invalid socket id");
        return false;
    }

    int id = hash_id_lookup(&mod_ptr->hash, socket_id);
    if (id >= 0)
    {
        node_socket::instance()->close(mod_ptr->svc_ctx, socket_id);
    }

    return true;
}

static bool _handle_ctrl_cmd_forward(gate_mod* mod_ptr, std::vector<std::string>& param_info)
{
    // check param num
    if (param_info.size() < 3)
    {
        log(mod_ptr->svc_ctx, "[gate] forward failed, the number of param must == 3");
        return false;
    }

    int socket_id = 0;
    try
    {
        socket_id = std::stoi(param_info[0]);
    }
    catch (...)
    {
        log(mod_ptr->svc_ctx, "[gate] forward failed, invalid socket id");
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
        log(mod_ptr->svc_ctx, "[gate] forward failed, invalid agent service handle");
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
        log(mod_ptr->svc_ctx, "[gate] forward failed, invalid client service handle");
        return false;
    }

    int idx = hash_id_lookup(&mod_ptr->hash, socket_id);
    if (idx >= 0)
    {
        auto agent = mod_ptr->connections.get()[idx];
        agent.agent_svc_handle = agent_svc_handle;
        agent.client_svc_handle = client_svc_handle;
    }

    return true;
}

static bool _handle_ctrl_cmd_broker(gate_mod* mod_ptr, std::vector<std::string>& param_info)
{
    // check param num
    if (param_info.size() < 1)
    {
        log(mod_ptr->svc_ctx, "[gate] forward failed, the number of param must == 1");
        return false;
    }

    std::string name_or_addr = param_info[0];
    mod_ptr->broker_svc_handle = service_manager::instance()->query_by_name(mod_ptr->svc_ctx, name_or_addr.c_str());

    return true;
}

static bool _handle_ctrl_cmd_start(gate_mod* mod_ptr, std::vector<std::string>& param_info)
{
    // check param num
    if (param_info.size() < 1)
    {
        log(mod_ptr->svc_ctx, "[gate] start failed, the number of param must == 1");
        return false;
    }

    int socket_id = 0;
    try
    {
        socket_id = std::stoi(param_info[0]);
    }
    catch (...)
    {
        log(mod_ptr->svc_ctx, "[gate] start failed, invalid socket id");
        return false;
    }

    int id = hash_id_lookup(&mod_ptr->hash, socket_id);
    if (id >= 0)
    {
        node_socket::instance()->start(mod_ptr->svc_ctx, socket_id);
    }

    return true;
}

static bool _handle_ctrl_cmd_close(gate_mod* mod_ptr)
{
    if (mod_ptr->listen_id >= 0)
    {
        node_socket::instance()->close(mod_ptr->svc_ctx, mod_ptr->listen_id);
        mod_ptr->listen_id = -1;
    }

    return true;
}

void handle_ctrl_cmd(gate_mod* mod_ptr, const char* msg, int sz)
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
        if (!_handle_ctrl_cmd_kick(mod_ptr, param_info))
        {
            log(mod_ptr->svc_ctx, "[gate] command : ", cmd_string.c_str());
        }
    }
    // cmd - "forward socket_id agent_svc_handle client_svc_handle"
    else if (cmd == "forward")
    {
        if (!_handle_ctrl_cmd_forward(mod_ptr, param_info))
        {
            log(mod_ptr->svc_ctx, "[gate] command : ", cmd_string.c_str());
        }
    }
    // cmd - "broker name_or_addr"
    else if (cmd == "broker")
    {
        if (!_handle_ctrl_cmd_broker(mod_ptr, param_info))
        {
            log(mod_ptr->svc_ctx, "[gate] command : ", cmd_string.c_str());
        }
    }
    // cmd - "start socket_id"
    else if (cmd == "start")
    {
        if (!_handle_ctrl_cmd_start(mod_ptr, param_info))
        {
            log(mod_ptr->svc_ctx, "[gate] command : ", cmd_string.c_str());
        }
    }
    // cmd - "close"
    else if (cmd == "close")
    {
        _handle_ctrl_cmd_close(mod_ptr);
    }
    else
    {
        log(mod_ptr->svc_ctx, "[gate] Unknown command : %s", cmd_string.c_str());
    }
}

} }
