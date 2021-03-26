local skynet = require "skynet"
local skynet_core = require "skynet.core"

---
--- launch service
function skynet.launch(...)
    local addr = skynet_core.command("LAUNCH", table.concat({ ... }, " "))
    if addr then
        return tonumber("0x" .. string.sub(addr, 2))
    end
end

---
--- kill service
--- @param name string|number
function skynet.kill(name)
    if type(name) == "number" then
        skynet.send(".launcher", "lua", "REMOVE", name, true)
        name = skynet.to_address(name)
    end
    skynet_core.command("KILL", name)
end

---
---
function skynet.abort()
    skynet_core.command("ABORT")
end

---
--- @param name string
function skynet.register(name)
    skynet_core.command("REG", name)
end

---
--- @param name string
--- @param handle
function skynet.name(name, handle)
    skynet_core.command("NAME", name .. " " .. skynet.to_address(handle))
end

local handle_service_message = skynet.handle_service_message

---
--- forward message by service message type translate map
--- @param forward_svc_msg_type_map table service message type map, key: origin service message type, forward service message type
--- @param start_func function
function skynet.forward_by_type(forward_svc_msg_type_map, start_func)
    -- set service message callback
    skynet_core.callback(function(svc_msg_type, msg, msg_sz, ...)
        local fowward_msg_type = forward_svc_msg_type_map[svc_msg_type]
        if fowward_msg_type then
            handle_service_message(fowward_msg_type, msg, msg_sz, ...)
        else
            local ok, err = pcall(handle_service_message, svc_msg_type, msg, msg_sz, ...)
            skynet_core.trash(msg, msg_sz)
            if not ok then
                error(err)
            end
        end
    end, true)

    --
    skynet.timeout(0, function()
        skynet.init_service(start_func)
    end)
end

---
--- @param f function filter function
--- @param start_func function
function skynet.filter(f, start_func)
    -- set service message callback
    skynet_core.callback(function(...)
        handle_service_message(f(...))
    end)
    --
    skynet.timeout(0, function()
        skynet.init_service(start_func)
    end)
end

---
--- set/get montior service
--- @param service string the monitor service
--- @param query boolean
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
