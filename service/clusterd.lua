--[[
    skynet cluster service

    all messages that need to cross process are delivered to this service,
    and then forward to the network by this service.

    cluster node naming:
    - cluster node must have a unique string name. it used to service reigster, discovery.

    features:
    -
]]

local skynet = require "skynet"
require "skynet.manager"
local cluster_core = require "skynet.cluster.core"

local tostring = tostring

local config_name = skynet.get_env("cluster")

local node_map = {}             -- cluster node map (k: node name, v: node address)
local cluster_config_map = {}   -- cluster config map (k: config name, v: config value)
local node_sender_svc_map = {}  -- cluster node sender service map (k: cluster node name, va: cluster sender service handle)
local connecting = {}           -- cluster node connecting info = { node_name = { namequery_thread = thread, channel = sender_svc }, }

--- the name of this cluster node (format: hostname+pid | nohost+pid)
local this_node_name = cluster_core.nodename()

---
--- open sender service
---@param t table cluster node channel table
---@param key string cluster node name
local function open_channel(t, key)
    -- check resolve, block
    local connecting_info = connecting[key]
    if connecting_info then
        local current_thread = coroutine.running()
        table.insert(connecting_info, current_thread)
        -- wait current thread
        skynet.wait(current_thread)
        return assert(connecting_info.sender_svc)
    end

    --
    connecting_info = {}
    connecting[key] = connecting_info

    --
    local node_address = node_map[key]
    -- node address is nil, block until cluster node address resloved
    if node_address == nil and not cluster_config_map.nowaiting then
        local current_thread = coroutine.running()
        assert(connecting_info.namequery_thread == nil)
        connecting_info.namequery_thread = current_thread
        skynet.log_info("Waiting for cluster node [" .. key .. "]")
        -- wait query cluster node success
        skynet.wait(current_thread)
        node_address = node_map[key]
    end

    --
    local sender_svc
    local ok, err
    --  cluster node address is provide or reslove success
    if node_address then
        local host, port = string.match(node_address, "([^:]+):(.*)$")
        sender_svc = node_sender_svc_map[key]
        if sender_svc == nil then
            sender_svc = skynet.newservice("cluster_sender", key, this_node_name, host, port)
            if node_sender_svc_map[key] then
                -- double check
                skynet.kill(sender_svc)
                sender_svc = node_sender_svc_map[key]
            else
                node_sender_svc_map[key] = sender_svc
            end
        end

        --
        ok = pcall(skynet.call, sender_svc, "lua", "changenode", host, port)
        if ok then
            t[key] = sender_svc
            connecting_info.sender_svc = sender_svc
        else
            err = string.format("changenode [%s] (%s:%s) failed", key, host, port)
        end
    else
        -- cluster node is down, or can't reslove
        err = string.format("cluster node [%s] is %s.", key, node_address == false and "down" or "absent")
    end

    --
    connecting[key] = nil
    --
    for _, thread in ipairs(connecting_info) do
        skynet.wakeup(thread)
    end

    --
    assert(ok, err)
    if node_map[key] ~= node_address then
        return open_channel(t, key)
    end
    return sender_svc
end

--- cluster node sender channel
local node_sender_channel = setmetatable({}, { __index = open_channel })

---
--- load cluster config
---
--- cluster config file example:
--- __nowaiting = true  -- If you turn this flag off, cluster.call would block when node name is absent
--- login_1 = "0.0.0.0:9000"
--- connect_1 = "0.0.0.0:9010"
--- gate_1 = "0.0.0.0:9020"
--- ...
---
---@param env
local function load_config(env)
    -- load cluster config file
    if env == nil then
        env = {}
        if config_name then
            local f = assert(io.open(config_name))
            local chunk = f:read("*a")
            f:close()
            assert(load(chunk, "@" .. config_name, "t", env))()
        end
    end

    --
    local reconnect_nodes = {}
    for node_name, node_address in pairs(env) do
        -- start with '__', e.g. __nowaiting
        if node_name:sub(1, 2) == "__" then
            node_name = node_name:sub(3)
            cluster_config_map[node_name] = node_address
            skynet.log_info(string.format("Config %s = %s", node_name, node_address))
        else
            -- cluster node config

            assert(node_address == false or type(node_address) == "string")
            -- address changed, reset connection then reconnect
            if node_map[node_name] ~= node_address then
                if rawget(node_sender_channel, node_name) then
                    node_sender_channel[node_name] = nil    -- reset connection
                    table.insert(reconnect_nodes, node_name)
                end
                node_map[node_name] = node_address
            end

            -- reslove cluster node
            local connecting_info = connecting[node_name]
            if connecting_info and connecting_info.namequery_thread and not cluster_config_map.nowaiting then
                skynet.log_info(string.format("Cluster node [%s] resloved : %s", node_name, node_address))
                skynet.wakeup(connecting_info.namequery_thread)
            end
        end
    end

    -- no block, wakeup all connecting request
    if cluster_config_map.nowaiting then
        for _, connecting_info in pairs(connecting) do
            if connecting_info.namequery_thread then
                skynet.wakeup(connecting_info.namequery_thread)
            end
        end
    end

    -- reconnect cluster node
    for _, node_name in ipairs(reconnect_nodes) do
        -- open_channel would block
        skynet.fork(open_channel, node_sender_channel, node_name)
    end
end

-- ----------------------------------------------
-- cluster service cmd
-- ----------------------------------------------

local CMD = {}

local proxy = {}

---
--- reload cluster config
---@param source
---@param config
function CMD.reload(source, config)
    load_config(config)
    skynet.ret_pack(nil)
end

---
--- start cluster socket listen
---@param source
---@param host string listen uri or ip (socket bind ip), e.g, "192.168.0.1:1234" or "192.168.0.1"
---@param port number listen port (socket bind port)
function CMD.listen(source, host, port)
    -- create net gate service
    local gate = skynet.newservice("gate")

    --
    if port == nil then
        local node_address = assert(node_map[host], host .. " is down")
        host, port = string.match(node_address, "([^:]+):(.*)$")
    end
    skynet.call(gate, "lua", "open", { address = host, port = port })
    skynet.ret_pack(nil)
end

---
--- get a cluster node sender
---@param source
---@param node string skynet cluster node
function CMD.sender(source, node)
    skynet.ret_pack(node_sender_channel[node])
end

---
---@param source
function CMD.senders(source)
    skynet.ret_pack(node_sender_svc_map)
end

---
--- create a cluster proxy
---@param source
---@param node string the skynet cluster node
---@param name string
function CMD.proxy(source, node, name)
    if name == nil then
        node, name = node:match("^([^@.]+)([@.].+)")
        if name == nil then
            error("Invalid name " .. tostring(node))
        end
    end

    --
    local fullname = node .. "." .. name
    local p = proxy[fullname]
    if p == nil then
        p = skynet.newservice("cluster_proxy", node, name)
        -- double check
        if proxy[fullname] then
            skynet.kill(p)
            p = proxy[fullname]
        else
            proxy[fullname] = p
        end
    end
    skynet.ret_pack(p)
end

local cluster_agent = {}    -- key: fd, value: service
local register_name = {}

local function clearnamecache()
    for fd, service in pairs(cluster_agent) do
        if type(service) == "number" then
            skynet.send(service, "lua", "namechange")
        end
    end
end

---
--- register the service within cluster node
---@param source
---@param node_name string cluster node name
---@param svc_handle number service handle
function CMD.register(source, node_name, svc_handle)
    assert(register_name[node_name] == nil)
    svc_handle = svc_handle or source
    local old_name = register_name[svc_handle]
    if old_name then
        register_name[old_name] = nil
        clearnamecache()
    end
    register_name[svc_handle] = node_name
    register_name[node_name] = svc_handle
    skynet.ret(nil)

    skynet.log_info(string.format("Register [%s] :%08x", node_name, svc_handle))
end

---
---
---@param source
---@param name string
function CMD.queryname(source, name)
    skynet.ret_pack(register_name[name])
end

---
---@param source
---@param subcmd
---@param fd
---@param msg
function CMD.socket(source, subcmd, fd, msg)
    if subcmd == "open" then
        skynet.log_info(string.format("socket accept from %s", msg))

        -- new cluster agent
        cluster_agent[fd] = false
        local agent = skynet.newservice("cluster_agent", skynet.self(), source, fd)
        local closed = cluster_agent[fd]
        cluster_agent[fd] = agent
        if closed then
            skynet.send(agent, "lua", "exit")
            cluster_agent[fd] = nil
        end
    else
        if subcmd == "close" or subcmd == "error" then
            -- close cluster agent
            local agent = cluster_agent[fd]
            if type(agent) == "boolean" then
                cluster_agent[fd] = true
            elseif agent then
                skynet.send(agent, "lua", "exit")
                cluster_agent[fd] = nil
            end
        else
            skynet.log_info(string.format("socket %s %d %s", subcmd, fd, msg or ""))
        end
    end
end

--
skynet.start(function()
    --
    load_config()

    -- set service message dispatch function
    skynet.dispatch("lua", function(session_id, src_svc_handle, cmd, ...)
        local f = assert(CMD[cmd])
        f(src_svc_handle, ...)
    end)
end)
