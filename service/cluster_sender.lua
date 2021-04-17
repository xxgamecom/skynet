--[[
]]

local skynet = require "skynet"
local socket_channel = require "skynet.socket_channel"
local socket = require "skynet.socket"
local cluster_core = require "skynet.cluster.core"

local channel
local session = 1
local node, nodename, init_host, init_port = ...

local CMD = {}

--
local function send_request(addr, msg, msg_sz)
    -- msg is a local pointer, cluster_core.pack_request will free it
    local current_session = session
    local req, new_session, padding = cluster_core.pack_request(addr, session, msg, msg_sz)
    session = new_session

    local trace_tag = skynet.trace_tag()
    if trace_tag then
        if trace_tag:sub(1, 1) ~= "(" then
            -- add nodename
            local new_tag = string.format("(%s-%s-%d)%s", nodename, node, session, trace_tag)
            skynet.tracelog(trace_tag, string.format("session %s", new_tag))
            trace_tag = new_tag
        end
        skynet.tracelog(trace_tag, string.format("cluster %s", node))
        channel:request(cluster_core.pack_trace(trace_tag))
    end
    return channel:request(req, current_session, padding)
end

function CMD.req(...)
    local ok, msg = pcall(send_request, ...)
    if ok then
        if type(msg) == "table" then
            skynet.ret(cluster_core.concat(msg))
        else
            skynet.ret(msg)
        end
    else
        skynet.log_error(msg)
        skynet.response()(false)
    end
end

---
---@param svc_addr
---@param msg
---@param msg_sz
function CMD.push(svc_addr, msg, msg_sz)
    local req, new_session, padding = cluster_core.pack_push(svc_addr, session, msg, msg_sz)
    if padding then
        -- is multi push
        session = new_session
    end

    channel:request(req, nil, padding)
end

local function read_response(sock)
    local msg_sz = socket.header(sock:read(2))
    local msg = sock:read(msg_sz)
    return cluster_core.unpack_response(msg)    -- session, ok, data, padding
end

---
---@param host
---@param port
function CMD.changenode(host, port)
    channel:changehost(host, tonumber(port))
    channel:connect(true)
    skynet.ret_pack(nil)
end

skynet.start(function()
    --
    channel = socket_channel.channel({
        host = init_host,
        port = tonumber(init_port),
        response = read_response,
        nodelay = true,
    })

    -- set "lua" service message dispatch function
    skynet.dispatch("lua", function(session_id, src_svc_handle, cmd, ...)
        local f = assert(CMD[cmd])
        f(...)
    end)
end)
