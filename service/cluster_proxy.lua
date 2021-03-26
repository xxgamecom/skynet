local skynet = require "skynet"
local cluster = require "skynet.cluster"
require "skynet.manager"

local node, address = ...

--
skynet.register_svc_msg_handler({
    msg_type_name = "system",
    msg_type = skynet.SERVICE_MSG_TYPE_SYSTEM,
    unpack = function(...)
        return ...
    end,
})

--
local forward_svc_msg_type_map = {
    [skynet.SERVICE_MSG_TYPE_SNAX] = skynet.SERVICE_MSG_TYPE_SYSTEM,
    [skynet.SERVICE_MSG_TYPE_LUA] = skynet.SERVICE_MSG_TYPE_SYSTEM,
    [skynet.SERVICE_MSG_TYPE_RESPONSE] = skynet.SERVICE_MSG_TYPE_RESPONSE, -- don't free response message
}

-- forward message
skynet.forward_by_type(forward_svc_msg_type_map, function()
    local clusterd = skynet.uniqueservice("clusterd")
    local n = tonumber(address)
    if n then
        address = n
    end
    local sender = skynet.call(clusterd, "lua", "sender", node)
    skynet.dispatch("system", function(session, source, msg, sz)
        if session == 0 then
            skynet.send(sender, "lua", "push", address, msg, sz)
        else
            skynet.ret(skynet.call_raw(sender, "lua", skynet.pack("req", address, msg, sz)))
        end
    end)
end)
