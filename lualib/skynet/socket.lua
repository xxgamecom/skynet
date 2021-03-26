local skynet = require "skynet"
local skynet_core = require "skynet.core"
local socketdriver = require "skynet.socketdriver"

local assert = assert

local BUFFER_LIMIT = 128 * 1024
local socket = {} -- api

-- store all socket object
local socket_pool = setmetatable({}, { __gc = function(p)
    for socket_id, v in pairs(p) do
        socketdriver.close(socket_id)
        p[socket_id] = nil
    end
end
})

local function wakeup(s)
    local co = s.co
    if co then
        s.co = nil
        skynet.wakeup(co)
    end
end

local function pause_socket(s, size)
    if s.pause then
        return
    end
    if size then
        skynet.log(string.format("Pause socket (%d) size : %d", s.socket_id, size))
    else
        skynet.log(string.format("Pause socket (%d)", s.socket_id))
    end
    socketdriver.pause(s.socket_id)
    s.pause = true
    skynet.yield()    -- there are subsequent socket messages in mqueue, maybe.
end

local function suspend(s)
    assert(not s.co)
    s.co = coroutine.running()
    if s.pause then
        skynet.log(string.format("Resume socket (%d)", s.socket_id))
        socketdriver.start(s.socket_id)
        skynet.wait(s.co)
        s.pause = nil
    else
        skynet.wait(s.co)
    end
    -- wakeup closing corouting every time suspend,
    -- because socket.close() will wait last socket buffer operation before clear the buffer.
    if s.closing then
        skynet.wakeup(s.closing)
    end
end

-- socket message handle
local socket_message = {}

-- SKYNET_SOCKET_EVENT_DATA = 1
socket_message[1] = function(socket_id, size, data)
    local s = socket_pool[socket_id]
    if s == nil then
        skynet.log("socket: drop package from " .. socket_id)
        socketdriver.drop(data, size)
        return
    end

    local sz = socketdriver.push(s.buffer, s.pool, data, size)
    local rr = s.read_required
    local rrt = type(rr)
    if rrt == "number" then
        -- read size
        if sz >= rr then
            s.read_required = nil
            if sz > BUFFER_LIMIT then
                pause_socket(s, sz)
            end
            wakeup(s)
        end
    else
        if s.buffer_limit and sz > s.buffer_limit then
            skynet.log(string.format("socket buffer overflow: fd=%d size=%d", socket_id, sz))
            socketdriver.close(socket_id)
            return
        end
        if rrt == "string" then
            -- read line
            if socketdriver.readline(s.buffer, nil, rr) then
                s.read_required = nil
                if sz > BUFFER_LIMIT then
                    pause_socket(s, sz)
                end
                wakeup(s)
            end
        elseif sz > BUFFER_LIMIT and not s.pause then
            pause_socket(s, sz)
        end
    end
end

-- SKYNET_SOCKET_EVENT_CONNECT = 2
socket_message[2] = function(socket_id, _, addr)
    local s = socket_pool[socket_id]
    if s == nil then
        return
    end
    -- log remote addr
    if not s.connected then
        -- resume may also post connect message
        s.connected = true
        wakeup(s)
    end
end

-- SKYNET_SOCKET_EVENT_CLOSE = 3
socket_message[3] = function(socket_id)
    local s = socket_pool[socket_id]
    if s == nil then
        socketdriver.close(socket_id)
        return
    end
    s.connected = false
    wakeup(s)
    if s.on_close then
        s.on_close(socket_id)
    end
end

-- SKYNET_SOCKET_EVENT_ACCEPT = 4
socket_message[4] = function(socket_id, newid, addr)
    local s = socket_pool[socket_id]
    if s == nil then
        socketdriver.close(newid)
        return
    end
    s.callback(newid, addr)
end

-- SKYNET_SOCKET_EVENT_ERROR = 5
socket_message[5] = function(socket_id, _, err)
    local s = socket_pool[socket_id]
    if s == nil then
        socketdriver.shutdown(socket_id)
        skynet.log("socket: error on unknown", socket_id, err)
        return
    end
    if s.callback then
        skynet.log("socket: accpet error:", err)
        return
    end
    if s.connected then
        skynet.log("socket: error on", socket_id, err)
    elseif s.connecting then
        s.connecting = err
    end
    s.connected = false
    socketdriver.shutdown(socket_id)

    wakeup(s)
end

-- SKYNET_SOCKET_EVENT_UDP = 6
socket_message[6] = function(socket_id, size, data, address)
    local s = socket_pool[socket_id]
    if s == nil or s.callback == nil then
        skynet.log("socket: drop udp package from " .. socket_id)
        socketdriver.drop(data, size)
        return
    end
    local str = skynet.tostring(data, size)
    skynet_core.trash(data, size)
    s.callback(str, address)
end

local function default_warning(socket_id, size)
    local s = socket_pool[socket_id]
    if not s then
        return
    end
    skynet.log(string.format("WARNING: %d K bytes need to send out (fd = %d)", size, socket_id))
end

-- SKYNET_SOCKET_EVENT_WARNING = 7
socket_message[7] = function(socket_id, size)
    local s = socket_pool[socket_id]
    if s then
        local warning = s.on_warning or default_warning
        warning(socket_id, size)
    end
end

skynet.register_protocol {
    msg_type_name = "socket",
    msg_type = skynet.SERVICE_MSG_TYPE_SOCKET, -- SERVICE_MSG_TYPE_SOCKET = 6
    unpack = socketdriver.unpack,
    dispatch = function(_, _, t, ...)
        socket_message[t](...)
    end
}

local function connect(socket_id, func)
    local newbuffer
    if func == nil then
        newbuffer = socketdriver.new_buffer()
    end
    local s = {
        socket_id = socket_id,
        buffer = newbuffer,
        pool = newbuffer and {},
        connected = false,
        connecting = true,
        read_required = false,  --
        co = false,             -- thread id
        callback = func,
        protocol = "TCP",
    }
    assert(not socket_pool[socket_id], "socket is not closed")
    socket_pool[socket_id] = s
    suspend(s)
    local err = s.connecting
    s.connecting = nil
    if s.connected then
        return socket_id
    else
        socket_pool[socket_id] = nil
        return nil, err
    end
end

function socket.open(addr, port)
    local socket_id = socketdriver.connect(addr, port)
    return connect(socket_id)
end

function socket.bind(os_fd)
    local socket_id = socketdriver.bind(os_fd)
    return connect(socket_id)
end

function socket.stdin()
    return socket.bind(0)
end

function socket.start(socket_id, func)
    socketdriver.start(socket_id)
    return connect(socket_id, func)
end

function socket.pause(socket_id)
    local s = socket_pool[socket_id]
    if s == nil or s.pause then
        return
    end
    pause_socket(s)
end

function socket.shutdown(socket_id)
    local s = socket_pool[socket_id]
    if s then
        -- the framework would send SKYNET_SOCKET_EVENT_CLOSE , need close(socket_id) later
        socketdriver.shutdown(socket_id)
    end
end

function socket.close_fd(socket_id)
    assert(socket_pool[socket_id] == nil, "Use socket.close instead")
    socketdriver.close(socket_id)
end

function socket.close(socket_id)
    local s = socket_pool[socket_id]
    if s == nil then
        return
    end
    socketdriver.close(socket_id)
    if s.connected then
        if s.co then
            -- reading this socket on another coroutine, so don't shutdown (clear the buffer) immediately
            -- wait reading coroutine read the buffer.
            assert(not s.closing)
            s.closing = coroutine.running()
            skynet.wait(s.closing)
        else
            suspend(s)
        end
        s.connected = false
    end
    assert(s.lock == nil or next(s.lock) == nil)
    socket_pool[socket_id] = nil
end

function socket.read(socket_id, sz)
    local s = socket_pool[socket_id]
    assert(s)
    if sz == nil then
        -- read some bytes
        local ret = socketdriver.readall(s.buffer, s.pool)
        if ret ~= "" then
            return ret
        end

        if not s.connected then
            return false, ret
        end
        assert(not s.read_required)
        s.read_required = 0
        suspend(s)
        ret = socketdriver.readall(s.buffer, s.pool)
        if ret ~= "" then
            return ret
        else
            return false, ret
        end
    end

    local ret = socketdriver.pop(s.buffer, s.pool, sz)
    if ret then
        return ret
    end
    if not s.connected then
        return false, socketdriver.readall(s.buffer, s.pool)
    end

    assert(not s.read_required)
    s.read_required = sz
    suspend(s)
    ret = socketdriver.pop(s.buffer, s.pool, sz)
    if ret then
        return ret
    else
        return false, socketdriver.readall(s.buffer, s.pool)
    end
end

function socket.readall(socket_id)
    local s = socket_pool[socket_id]
    assert(s)
    if not s.connected then
        local r = socketdriver.readall(s.buffer, s.pool)
        return r ~= "" and r
    end
    assert(not s.read_required)
    s.read_required = true
    suspend(s)
    assert(s.connected == false)
    return socketdriver.readall(s.buffer, s.pool)
end

function socket.readline(socket_id, sep)
    sep = sep or "\n"
    local s = socket_pool[socket_id]
    assert(s)
    local ret = socketdriver.readline(s.buffer, s.pool, sep)
    if ret then
        return ret
    end
    if not s.connected then
        return false, socketdriver.readall(s.buffer, s.pool)
    end
    assert(not s.read_required)
    s.read_required = sep
    suspend(s)
    if s.connected then
        return socketdriver.readline(s.buffer, s.pool, sep)
    else
        return false, socketdriver.readall(s.buffer, s.pool)
    end
end

function socket.block(socket_id)
    local s = socket_pool[socket_id]
    if not s or not s.connected then
        return false
    end
    assert(not s.read_required)
    s.read_required = 0
    suspend(s)
    return s.connected
end

socket.write = assert(socketdriver.send)
socket.lwrite = assert(socketdriver.lsend)
socket.header = assert(socketdriver.header)

function socket.invalid(socket_id)
    return socket_pool[socket_id] == nil
end

function socket.disconnected(socket_id)
    local s = socket_pool[socket_id]
    if s then
        return not (s.connected or s.connecting)
    end
end

function socket.listen(host, port, backlog)
    if port == nil then
        host, port = string.match(host, "([^:]+):(.+)$")
        port = tonumber(port)
    end
    return socketdriver.listen(host, port, backlog)
end

function socket.lock(socket_id)
    local s = socket_pool[socket_id]
    assert(s)
    local lock_set = s.lock
    if not lock_set then
        lock_set = {}
        s.lock = lock_set
    end
    if #lock_set == 0 then
        lock_set[1] = true
    else
        local co = coroutine.running()
        table.insert(lock_set, co)
        skynet.wait(co)
    end
end

function socket.unlock(socket_id)
    local s = socket_pool[socket_id]
    assert(s)
    local lock_set = assert(s.lock)
    table.remove(lock_set, 1)
    local co = lock_set[1]
    if co then
        skynet.wakeup(co)
    end
end

-- abandon use to forward socket id to other service
-- you must call socket.start(socket_id) later in other service
function socket.abandon(socket_id)
    local s = socket_pool[socket_id]
    if s then
        s.connected = false
        wakeup(s)
        socket_pool[socket_id] = nil
    end
end

function socket.limit(socket_id, limit)
    local s = assert(socket_pool[socket_id])
    s.buffer_limit = limit
end

---------------------- UDP

local function create_udp_object(socket_id, cb)
    assert(not socket_pool[socket_id], "socket is not closed")
    socket_pool[socket_id] = {
        socket_id = socket_id,
        connected = true,
        protocol = "UDP",
        callback = cb,
    }
end

function socket.udp(callback, host, port)
    local socket_id = socketdriver.udp(host, port)
    create_udp_object(socket_id, callback)
    return socket_id
end

function socket.udp_connect(socket_id, addr, port, callback)
    local obj = socket_pool[socket_id]
    if obj then
        assert(obj.protocol == "UDP")
        if callback then
            obj.callback = callback
        end
    else
        create_udp_object(socket_id, callback)
    end
    socketdriver.udp_connect(socket_id, addr, port)
end

socket.sendto = assert(socketdriver.udp_send)
socket.udp_address = assert(socketdriver.udp_address)
socket.netstat = assert(socketdriver.info)

function socket.warning(socket_id, callback)
    local obj = socket_pool[socket_id]
    assert(obj)
    obj.on_warning = callback
end

function socket.onclose(socket_id, callback)
    local obj = socket_pool[socket_id]
    assert(obj)
    obj.on_close = callback
end

return socket
