--[[
    clusterd service api & helper
]]

local skynet = require "skynet"

local clusterd
local cluster = {}
local sender = {}
local task_queue = {}

local function request_sender(q, node)
    local ok, c = pcall(skynet.call, clusterd, "lua", "sender", node)
    if not ok then
        skynet.log_info(c)
        c = nil
    end
    -- run tasks in queue
    local thread = coroutine.running()
    q.thread = thread
    q.sender = c
    for _, thread in ipairs(q) do
        if type(thread) == "table" then
            if c then
                skynet.send(c, "lua", "push", thread[1], skynet.pack(table.unpack(thread, 2, thread.n)))
            end
        else
            skynet.wakeup(thread)
            skynet.wait(thread)
        end
    end
    task_queue[node] = nil
    sender[node] = c
end

local function get_queue(t, node)
    local q = {}
    t[node] = q
    skynet.fork(request_sender, q, node)
    return q
end

setmetatable(task_queue, { __index = get_queue })

local function get_sender(node)
    local s = sender[node]
    if not s then
        local q = task_queue[node]
        local thread = coroutine.running()
        table.insert(q, thread)
        skynet.wait(thread)
        skynet.wakeup(q.thread)
        return q.sender
    end
    return s
end

---
---@param node
---@param address
function cluster.call(node, address, ...)
    -- skynet.pack(...) will free by cluster.core.pack_request
    return skynet.call(get_sender(node), "lua", "req", address, skynet.pack(...))
end

---
---@param node
---@param address
function cluster.send(node, address, ...)
    -- push is the same with req, but no response
    local s = sender[node]
    if not s then
        table.insert(task_queue[node], table.pack(address, ...))
    else
        skynet.send(sender[node], "lua", "push", address, skynet.pack(...))
    end
end

---
--- start cluster service, listen on port
---@param uri_or_port string|number the listen uri or port (socket bind ip and port), e.g, "192.168.0.1:1234" or 1234
function cluster.open(uri_or_port)
    if type(uri_or_port) == "string" then
        skynet.call(clusterd, "lua", "listen", uri_or_port)
    else
        skynet.call(clusterd, "lua", "listen", "0.0.0.0", uri_or_port)
    end
end

---
---@param config
function cluster.reload(config)
    skynet.call(clusterd, "lua", "reload", config)
end

---
--- start a cluster proxy
---@param node
---@param name
function cluster.proxy(node, name)
    return skynet.call(clusterd, "lua", "proxy", node, name)
end

---
---@param node
---@param name
---@param address
function cluster.snax(node, name, address)
    local snax = require "skynet.snax"
    if not address then
        address = cluster.call(node, ".service", "QUERY", "snaxd", name)
    end
    local handle = skynet.call(clusterd, "lua", "proxy", node, address)
    return snax.bind(handle, name)
end

---
--- register the service within cluster node
---@param node_name string cluster node name
---@param svc_handle number service handle
function cluster.register(node_name, svc_handle)
    assert(type(node_name) == "string")
    assert(svc_handle == nil or type(svc_handle) == "number")
    return skynet.call(clusterd, "lua", "register", node_name, svc_handle)
end

---
---@param node
---@param name
function cluster.query(node, name)
    return skynet.call(get_sender(node), "lua", "req", 0, skynet.pack(name))
end

-- auto create clusterd service
skynet.init(function()
    clusterd = skynet.uniqueservice("clusterd")
end)

return cluster
