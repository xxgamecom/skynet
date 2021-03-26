--
--
--

local skynet = require "skynet"
require "skynet.manager"
local snax = require "skynet.snax"

local pairs = pairs
local ipairs = ipairs
local pcall = pcall
local type = type
local tostring = tostring

local service = {}

local function request(name, func, ...)
    local ok, handle = pcall(func, ...)
    local s = service[name]
    assert(type(s) == "table")
    if ok then
        service[name] = handle
    else
        service[name] = tostring(handle)
    end

    for _, v in ipairs(s) do
        skynet.wakeup(v.co)
    end

    if ok then
        return handle
    else
        error(tostring(handle))
    end
end

local function wait_for(name, func, ...)
    local s = service[name]
    if type(s) == "number" then
        return s
    end
    local co = coroutine.running()

    if s == nil then
        s = {}
        service[name] = s
    elseif type(s) == "string" then
        error(s)
    end

    assert(type(s) == "table")

    local session_id, src_svc_handle = skynet.context()

    if s.launch == nil and func then
        s.launch = {
            session = session_id,
            source = src_svc_handle,
            co = co,
        }
        return request(name, func, ...)
    end

    table.insert(s, {
        co = co,
        session = session_id,
        source = src_svc_handle,
    })
    skynet.wait()
    s = service[name]
    if type(s) == "string" then
        error(s)
    end
    assert(type(s) == "number")
    return s
end

local function read_name(service_name)
    if string.byte(service_name) == 64 then
        -- '@'
        return string.sub(service_name, 2)
    else
        return service_name
    end
end

local CMD = {}

function CMD.LAUNCH(service_name, subname, ...)
    local realname = read_name(service_name)

    if realname == "snaxd" then
        return wait_for(service_name .. "." .. subname, snax.rawnewservice, subname, ...)
    else
        return wait_for(service_name, skynet.newservice, realname, subname, ...)
    end
end

function CMD.QUERY(service_name, subname)
    local realname = read_name(service_name)

    if realname == "snaxd" then
        return wait_for(service_name .. "." .. subname)
    else
        return wait_for(service_name)
    end
end

local function list_service()
    local result = {}
    for k, v in pairs(service) do
        if type(v) == "string" then
            v = "Error: " .. v
        elseif type(v) == "table" then
            local querying = {}
            if v.launch then
                local session = skynet.task(v.launch.co)
                local launching_address = skynet.call(".launcher", "lua", "QUERY", session)
                if launching_address then
                    table.insert(querying, "Init as " .. skynet.to_address(launching_address))
                    table.insert(querying, skynet.call(launching_address, "debug", "TASK", "init"))
                    table.insert(querying, "Launching from " .. skynet.to_address(v.launch.source))
                    table.insert(querying, skynet.call(v.launch.source, "debug", "TASK", v.launch.session))
                end
            end
            if #v > 0 then
                table.insert(querying, "Querying:")
                for _, detail in ipairs(v) do
                    table.insert(querying, skynet.to_address(detail.source) .. " " .. tostring(skynet.call(detail.source, "debug", "TASK", detail.session)))
                end
            end
            v = table.concat(querying, "\n")
        else
            v = skynet.to_address(v)
        end

        result[k] = v
    end

    return result
end

local function register_global()
    function CMD.GLAUNCH(name, ...)
        local global_name = "@" .. name
        return CMD.LAUNCH(global_name, ...)
    end

    function CMD.GQUERY(name, ...)
        local global_name = "@" .. name
        return CMD.QUERY(global_name, ...)
    end

    local mgr = {}

    function CMD.REPORT(m)
        mgr[m] = true
    end

    local function add_list(all, m)
        local result = skynet.call(m, "lua", "LIST")
        for k, v in pairs(result) do
            all[k .. "@0"] = v
        end
    end

    function CMD.LIST()
        local result = {}
        for k in pairs(mgr) do
            pcall(add_list, result, k)
        end
        local l = list_service()
        for k, v in pairs(l) do
            result[k] = v
        end
        return result
    end
end

skynet.start(function()
    skynet.dispatch("lua", function(session, address, cmd, ...)
        local f = CMD[cmd]
        if f == nil then
            skynet.ret(skynet.pack(nil, "Invalid command " .. cmd))
            return
        end

        local ok, r = pcall(f, ...)

        if ok then
            skynet.ret(skynet.pack(r))
        else
            skynet.ret(skynet.pack(nil, r))
        end
    end)
    local handle = skynet.localname(".service")
    if handle then
        skynet.log(".service is already register by ", skynet.to_address(handle))
        skynet.exit()
    else
        skynet.register(".service")
    end

    register_global()
end)
