--[[
    skynet service manager

    - launch(), launch a new service instance
    - kill(), kill a service instance
    - abort(), kill all service
    - register(), register current service name
    - register_name(), register service name
    - forward_by_type(),
    - filter(),
    - monitor(), monitor service exit
]]

local skynet = require "skynet"
local skynet_core = require "skynet.core"

---
--- launch a new service instance
---@generic T
---@vararg T
---@return number service handle, e,g, 1,2,3,...
function skynet.launch(...)
    -- addr format: ":00000001", ...
    local addr = skynet_core.command("LAUNCH", table.concat({ ... }, " "))
    -- convert to number, e,g, 1,2,3,...
    if addr then
        return tonumber("0x" .. string.sub(addr, 2))
    end
end

---
--- kill a service instance
---@param addr_or_handle string|number
function skynet.kill(addr_or_handle)
    -- service handle
    if type(addr_or_handle) == "number" then
        skynet.send(".launcher", "lua", "REMOVE", addr_or_handle, true)
        addr_or_handle = skynet.to_address(addr_or_handle)
    end

    -- kill service instance
    skynet_core.command("KILL", addr_or_handle)
end

---
--- kill all service instance
function skynet.abort()
    skynet_core.command("ABORT")
end

---
--- register `current` service local name. for the service, can set multi service local name.
---@param svc_name string service local name, start with '.', e,g, ".launcher" ...
function skynet.register(svc_name)
    skynet_core.command("REGISTER", svc_name)
end

---
--- register service local name.
---@param svc_name string service local name
---@param svc_handle number service handle, e.g, 1,2,3...
function skynet.register_name(svc_name, svc_handle)
    local svc_addr = skynet.to_address(svc_handle)
    skynet_core.command("REGISTER_NAME", svc_name .. " " .. svc_addr)
end

local handle_service_message = skynet.handle_service_message

---
--- forward message by service message type translate map
---@param forward_svc_msg_type_map table service message type map, key: origin service message type, forward service message type
---@param start_func function
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
---@param f function filter function
---@param start_func function
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
---@param service string the monitor service
---@param is_query boolean is query
function skynet.monitor(service, is_query)
    local monitor
    if is_query then
        monitor = skynet.queryservice(true, service)
    else
        monitor = skynet.uniqueservice(true, service)
    end
    assert(monitor, "Monitor launch failed")

    -- monitor
    skynet_core.command("MONITOR", string.format(":%08x", monitor))
    return monitor
end

return skynet
