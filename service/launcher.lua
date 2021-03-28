local skynet = require "skynet"
local skynet_core = require "skynet.core"
require "skynet.manager"

local string = string
local pairs = pairs
local pcall = pcall
local tonumber = tonumber

local services = {}
local instance = {} -- for confirm (function CMD.LAUNCH / CMD.ERROR / CMD.LAUNCHOK)
local launch_session = {} -- for CMD.QUERY, service_address -> session

local function handle_to_address(handle)
    return tonumber("0x" .. string.sub(handle, 2))
end

local NORET = {}

local CMD = {}

function CMD.LIST()
    local list = {}
    for k, v in pairs(services) do
        list[skynet.to_address(k)] = v
    end
    return list
end

function CMD.STAT()
    local list = {}
    for k, v in pairs(services) do
        local ok, stat = pcall(skynet.call, k, "debug", "STAT")
        if not ok then
            stat = string.format("ERROR (%s)", v)
        end
        list[skynet.to_address(k)] = stat
    end
    return list
end

function CMD.KILL(_, handle)
    handle = handle_to_address(handle)
    skynet.kill(handle)
    local ret = { [skynet.to_address(handle)] = tostring(services[handle]) }
    services[handle] = nil
    return ret
end

function CMD.MEM()
    local list = {}
    for k, v in pairs(services) do
        local ok, kb = pcall(skynet.call, k, "debug", "MEM")
        if not ok then
            list[skynet.to_address(k)] = string.format("ERROR (%s)", v)
        else
            list[skynet.to_address(k)] = string.format("%.2f Kb (%s)", kb, v)
        end
    end
    return list
end

function CMD.GC()
    for k, v in pairs(services) do
        skynet.send(k, "debug", "GC")
    end
    return CMD.MEM()
end

function CMD.REMOVE(_, handle, kill)
    services[handle] = nil
    local response = instance[handle]
    if response then
        -- instance is dead
        response(not kill)    -- return nil to caller of newservice, when kill == false
        instance[handle] = nil
        launch_session[handle] = nil
    end

    -- don't return (skynet.ret) because the handle may exit
    return NORET
end

local function launch_service(service, ...)
    local param = table.concat({ ... }, " ")
    local inst = skynet.launch(service, param)
    local session_id = skynet.context()
    local response = skynet.response()
    if inst then
        services[inst] = service .. " " .. param
        instance[inst] = response
        launch_session[inst] = session_id
    else
        response(false)
        return
    end
    return inst
end

function CMD.LAUNCH(_, service, ...)
    launch_service(service, ...)
    return NORET
end

function CMD.LOGLAUNCH(_, service, ...)
    local inst = launch_service(service, ...)
    if inst then
        skynet_core.command("LOG_ON", skynet.to_address(inst))
    end
    return NORET
end

function CMD.ERROR(address)
    -- see serivce-src/service_lua.c
    -- init failed
    local response = instance[address]
    if response then
        response(false)
        launch_session[address] = nil
        instance[address] = nil
    end
    services[address] = nil
    return NORET
end

function CMD.LAUNCHOK(address)
    -- init notice
    local response = instance[address]
    if response then
        response(true, address)
        instance[address] = nil
        launch_session[address] = nil
    end

    return NORET
end

function CMD.QUERY(_, request_session)
    for address, session in pairs(launch_session) do
        if session == request_session then
            return address
        end
    end
end

-- for historical reasons, launcher support text command (for C service)

skynet.register_svc_msg_handler({
    msg_type_name = "text",
    msg_type = skynet.SERVICE_MSG_TYPE_TEXT,
    unpack = skynet.tostring,
    dispatch = function(session, address, cmd)
        if cmd == "" then
            CMD.LAUNCHOK(address)
        elseif cmd == "ERROR" then
            CMD.ERROR(address)
        else
            error("Invalid text command " .. cmd)
        end
    end,
})

-- set "lua" service message dispatch function
skynet.dispatch("lua", function(session, address, cmd, ...)
    cmd = string.upper(cmd)
    local f = CMD[cmd]
    if f then
        local ret = f(address, ...)
        if ret ~= NORET then
            skynet.ret_pack(ret)
        end
    else
        skynet.ret_pack({ "Unknown command" })
    end
end)

--
skynet.start(function()

end)
