local skynet = require "skynet"
local cluster = require "skynet.cluster"
require "skynet.manager"    -- inject skynet.forward_type

local node, address = ...

skynet.register_protocol {
    msg_ptype_name = "system",
    msg_ptype = skynet.MSG_PTYPE_SYSTEM,
    unpack = function(...)
        return ...
    end,
}

local forward_map = {
    [skynet.MSG_PTYPE_SNAX] = skynet.MSG_PTYPE_SYSTEM,
    [skynet.MSG_PTYPE_LUA] = skynet.MSG_PTYPE_SYSTEM,
    [skynet.MSG_PTYPE_RESPONSE] = skynet.MSG_PTYPE_RESPONSE, -- don't free response message
}

skynet.forward_type(forward_map, function()
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
            skynet.ret(skynet.rawcall(sender, "lua", skynet.pack("req", address, msg, sz)))
        end
    end)
end)
