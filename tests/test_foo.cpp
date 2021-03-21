#include <iostream>
#include <string>

static void _parse_param(char* msg, int sz, int cmd_sz)
{
    while (cmd_sz < sz)
    {
        if (msg[cmd_sz] != ' ')
            break;
        ++cmd_sz;
    }

    int i;
    for (i = cmd_sz; i < sz; i++)
    {
        msg[i - cmd_sz] = msg[i];
    }
    msg[i - cmd_sz] = '\0';
}

const char* msg = "forward 1 2 3";
int sz = ::strlen(msg);

int main()
{
    char tmp[sz + 1];
    ::memcpy(tmp, msg, sz);
    tmp[sz] = '\0';

    int i;
    char* cmd = tmp;
    for (i = 0; i < sz; i++)
    {
        if (cmd[i] == ' ')
        {
            break;
        }
    }

    // cmd - "kick socket_id"
    if (::memcmp(cmd, "kick", i) == 0)
    {
        _parse_param(tmp, sz, i);

        int uid = strtol(cmd , NULL, 10);
        return 0;
    }

    // cmd - "forward socket_id agent_svc_handle client_svc_handle"
    if (::memcmp(cmd, "forward", i) == 0)
    {
        _parse_param(tmp, sz, i);

        char* client = tmp;
        char* idstr = ::strsep(&client, " ");
        if (client == nullptr)
            return 0;

        int id = ::strtol(idstr, NULL, 10);

        char* agent = ::strsep(&client, " ");
        if (client == nullptr)
            return 0;

        uint32_t agent_svc_handle = ::strtoul(agent + 1, NULL, 16);
        uint32_t client_svc_handle = ::strtoul(client + 1, NULL, 16);
        return 0;
    }

    if (::memcmp(cmd, "broker", i) == 0)
    {
        _parse_param(tmp, sz, i);
        return 0;
    }
    if (::memcmp(cmd, "start", i) == 0)
    {
        _parse_param(tmp, sz, i);
        int uid = ::strtol(cmd, NULL, 10);
        return 0;
    }
    if (::memcmp(cmd, "close", i) == 0)
    {
        return 0;
    }

    return 0;
}