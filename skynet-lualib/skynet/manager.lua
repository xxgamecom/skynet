local skynet = require "skynet"
local skynet_core = require "skynet.core"

function skynet.launch(...)
    local addr = skynet_core.command("LAUNCH", table.concat({...}," "))
    if addr then
        return tonumber("0x" .. string.sub(addr , 2))
    end
end

function skynet.kill(name)
    if type(name) == "number" then
        skynet.send(".launcher","lua","REMOVE",name, true)
        name = skynet.address(name)
    end
    skynet_core.command("KILL",name)
end

function skynet.abort()
    skynet_core.command("ABORT")
end

-- register service name
function skynet.register(name)
    skynet_core.command("REG", name)
end

function skynet.name(name, handle)
    skynet_core.command("NAME", name .. " " .. skynet.address(handle))
end

local dispatch_message = skynet.dispatch_message

function skynet.forward_type(map, start_func)
    skynet_core.callback(function(ptype, msg, sz, ...)
        local prototype = map[ptype]
        if prototype then
            dispatch_message(prototype, msg, sz, ...)
        else
            local ok, err = pcall(dispatch_message, ptype, msg, sz, ...)
            skynet_core.trash(msg, sz)
            if not ok then
                error(err)
            end
        end
    end, true)
    skynet.timeout(0, function()
        skynet.init_service(start_func)
    end)
end

function skynet.filter(f ,start_func)
    skynet_core.callback(function(...)
        dispatch_message(f(...))
    end)
    skynet.timeout(0, function()
        skynet.init_service(start_func)
    end)
end

function skynet.monitor(service, query)
    local monitor
    if query then
        monitor = skynet.queryservice(true, service)
    else
        monitor = skynet.uniqueservice(true, service)
    end
    assert(monitor, "Monitor launch failed")
    skynet_core.command("MONITOR", string.format(":%08x", monitor))
    return monitor
end

return skynet
