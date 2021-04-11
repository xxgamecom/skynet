--
--
--

local skynet = require "skynet"
local gateserver = require "snax.gateserver"

local watchdog           --
local connection = {}    -- fd -> connection : { fd , client, agent , ip, mode }
local forwarding = {}    -- agent -> connection

skynet.register_svc_msg_handler({
    msg_type_name = "client",
    msg_type = skynet.SERVICE_MSG_TYPE_CLIENT,
})

local CMD = {}

local function unforward(c)
    if c.agent then
        forwarding[c.agent] = nil
        c.agent = nil
        c.client = nil
    end
end

local function close_fd(fd)
    local c = connection[fd]
    if c then
        unforward(c)
        connection[fd] = nil
    end
end

--
function CMD.forward(source, fd, client, address)
    local c = assert(connection[fd])
    unforward(c)
    c.client = client or 0
    c.agent = address or source
    forwarding[c.agent] = c
    gateserver.openclient(fd)
end

function CMD.accept(source, fd)
    local c = assert(connection[fd])
    unforward(c)
    gateserver.openclient(fd)
end

function CMD.kick(source, fd)
    gateserver.closeclient(fd)
end

local handler = {}

---
--- gateserver open callback
---@param source
---@param conf
function handler.open(source, conf)
    watchdog = conf.watchdog or source
end

function handler.message(fd, msg, sz)
    -- recv a package, forward it
    local c = connection[fd]
    local agent = c.agent
    if agent then
        -- It's safe to redirect msg directly , gateserver framework will not free msg.
        skynet.redirect(agent, c.client, "client", fd, msg, sz)
    else
        skynet.send(watchdog, "lua", "socket", "data", fd, skynet.tostring(msg, sz))
        -- skynet.tostring will copy msg to a string, so we must free msg here.
        skynet.trash(msg, sz)
    end
end

---
--- connect callback (accept remote client)
---@param fd number socket id
---@param addr string 'ip:port'
function handler.connect(fd, addr)
    local c = {
        fd = fd,
        ip = addr,
    }
    connection[fd] = c
    skynet.send(watchdog, "lua", "socket", "open", fd, addr)
end

---
--- disconnect callback
---@param fd number socket id
function handler.disconnect(fd)
    close_fd(fd)
    skynet.send(watchdog, "lua", "socket", "close", fd)
end

---
--- client error
---@param fd number
---@param msg
function handler.error(fd, msg)
    close_fd(fd)
    skynet.send(watchdog, "lua", "socket", "error", fd, msg)
end

---
---@param fd number
---@param size
function handler.warning(fd, size)
    skynet.send(watchdog, "lua", "socket", "warning", fd, size)
end

---
---
---@param cmd
---@param source
function handler.command(cmd, source, ...)
    local f = assert(CMD[cmd])
    return f(source, ...)
end

-- start gateserver
gateserver.start(handler)
