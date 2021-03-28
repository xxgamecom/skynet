local skynet = require "skynet"
local skynet_core = require "skynet.core"
local socket_core = require "skynet.socket.core"

local pairs = pairs
local assert = assert
local tonumber = tonumber

local BUFFER_LIMIT = 128 * 1024

local socket = {
    -- socket event
    SKYNET_SOCKET_EVENT_DATA = 1,
    SKYNET_SOCKET_EVENT_CONNECT = 2,
    SKYNET_SOCKET_EVENT_CLOSE = 3,
    SKYNET_SOCKET_EVENT_ACCEPT = 4,
    SKYNET_SOCKET_EVENT_ERROR = 5,
    SKYNET_SOCKET_EVENT_UDP = 6,
    SKYNET_SOCKET_EVENT_WARNING = 7,
}

-- store all socket object
local socket_pool = setmetatable({}, { __gc = function(p)
    for socket_id, v in pairs(p) do
        socket_core.close(socket_id)
        p[socket_id] = nil
    end
end })

--- wakeup socket thread
local function wakeup_socket(sock_obj)
    local thread = sock_obj.thread
    if thread then
        sock_obj.thread = nil
        skynet.wakeup(thread)
    end
end

--- pause socket thread
local function pause_socket(sock_obj, size)
    -- has paused
    if sock_obj.is_pause then
        return
    end

    -- pause socket thread
    if size then
        skynet.log(string.format("Pause socket (%d) size : %d", sock_obj.socket_id, size))
    else
        skynet.log(string.format("Pause socket (%d)", sock_obj.socket_id))
    end
    socket_core.pause(sock_obj.socket_id)
    sock_obj.is_pause = true
    skynet.yield()    -- there are subsequent socket messages in mqueue, maybe.
end

--- suspend the socket thread
---@param sock_obj table socket object
local function suspend_socket(sock_obj)
    assert(not sock_obj.thread)
    sock_obj.thread = coroutine.running()
    if sock_obj.is_pause then
        skynet.log(string.format("Resume socket (%d)", sock_obj.socket_id))
        socket_core.start(sock_obj.socket_id)
        skynet.wait(sock_obj.thread)
        sock_obj.is_pause = nil
    else
        skynet.wait(sock_obj.thread)
    end

    -- wakeup closing thread every time suspend,
    -- because socket.close() will wait last socket buffer operation before clear the buffer.
    if sock_obj.closing_thread then
        skynet.wakeup(sock_obj.closing_thread)
    end
end

---
---@param socket_id
---@param func function callback function
---@return
local function connect(socket_id, func)
    local newbuffer
    if func == nil then
        newbuffer = socket_core.new_buffer()
    end

    -- create socket object and add to pool
    local sock_obj = {
        socket_id = socket_id,
        buffer = newbuffer,
        buffer_pool = newbuffer and {},
        connected = false, --
        connecting = true, --
        read_required = false, --
        thread = false, -- socket thread, false means
        callback = func, --
        protocol = "TCP", --
    }
    assert(not socket_pool[socket_id], "socket is not closed")
    socket_pool[socket_id] = sock_obj

    --
    suspend_socket(sock_obj)

    --
    local err = sock_obj.connecting
    sock_obj.connecting = nil
    if sock_obj.connected then
        return socket_id
    else
        socket_pool[socket_id] = nil
        return nil, err
    end
end

---
---@param addr
---@param port
function socket.open(addr, port)
    local socket_id = socket_core.connect(addr, port)
    return connect(socket_id)
end

---
---@param os_fd
function socket.bind(os_fd)
    local socket_id = socket_core.bind(os_fd)
    return connect(socket_id)
end

---
---
function socket.stdin()
    return socket.bind(0)
end

---
---@param socket_id
---@param func
function socket.start(socket_id, func)
    socket_core.start(socket_id)
    return connect(socket_id, func)
end

---
---@param socket_id
function socket.pause(socket_id)
    local sock_obj = socket_pool[socket_id]
    if sock_obj == nil or sock_obj.is_pause then
        return
    end
    pause_socket(sock_obj)
end

---
---@param socket_id
function socket.shutdown(socket_id)
    local sock_obj = socket_pool[socket_id]
    if sock_obj then
        -- the framework would send SKYNET_SOCKET_EVENT_CLOSE , need close(socket_id) later
        socket_core.shutdown(socket_id)
    end
end

---
---@param socket_id
function socket.close(socket_id)
    local sock_obj = socket_pool[socket_id]
    if sock_obj == nil then
        return
    end

    socket_core.close(socket_id)
    if sock_obj.connected then
        if sock_obj.thread then
            -- reading this socket on another coroutine, so don't shutdown (clear the buffer) immediately
            -- wait reading coroutine read the buffer.
            assert(not sock_obj.closing_thread)
            sock_obj.closing_thread = coroutine.running()
            skynet.wait(sock_obj.closing_thread)
        else
            suspend_socket(sock_obj)
        end
        sock_obj.connected = false
    end
    assert(sock_obj.lock_set == nil or next(sock_obj.lock_set) == nil)
    socket_pool[socket_id] = nil
end

---
---@param socket_id number
---@param sz number read size
function socket.read(socket_id, sz)
    local sock_obj = socket_pool[socket_id]
    assert(sock_obj)

    --
    if sz == nil then
        -- read some bytes
        local ret = socket_core.read_all(sock_obj.buffer, sock_obj.buffer_pool)
        if ret ~= "" then
            return ret
        end

        if not sock_obj.connected then
            return false, ret
        end
        assert(not sock_obj.read_required)
        sock_obj.read_required = 0
        suspend_socket(sock_obj)
        ret = socket_core.read_all(sock_obj.buffer, sock_obj.buffer_pool)
        if ret ~= "" then
            return ret
        else
            return false, ret
        end
    end

    --
    local ret = socket_core.pop_buffer(sock_obj.buffer, sock_obj.buffer_pool, sz)
    if ret then
        return ret
    end
    if not sock_obj.connected then
        return false, socket_core.read_all(sock_obj.buffer, sock_obj.buffer_pool)
    end

    assert(not sock_obj.read_required)
    sock_obj.read_required = sz
    suspend_socket(sock_obj)
    ret = socket_core.pop_buffer(sock_obj.buffer, sock_obj.buffer_pool, sz)
    if ret then
        return ret
    end

    return false, socket_core.read_all(sock_obj.buffer, sock_obj.buffer_pool)
end

---
---@param socket_id
function socket.readall(socket_id)
    local sock_obj = socket_pool[socket_id]
    assert(sock_obj)
    if not sock_obj.connected then
        local r = socket_core.read_all(sock_obj.buffer, sock_obj.buffer_pool)
        return r ~= "" and r
    end
    assert(not sock_obj.read_required)
    sock_obj.read_required = true
    suspend_socket(sock_obj)
    assert(sock_obj.connected == false)
    return socket_core.read_all(sock_obj.buffer, sock_obj.buffer_pool)
end

---
---@param socket_id
---@param sep
function socket.readline(socket_id, sep)
    sep = sep or "\n"
    local sock_obj = socket_pool[socket_id]
    assert(sock_obj)
    local ret = socket_core.read_line(sock_obj.buffer, sock_obj.buffer_pool, sep)
    if ret then
        return ret
    end
    if not sock_obj.connected then
        return false, socket_core.read_all(sock_obj.buffer, sock_obj.buffer_pool)
    end
    assert(not sock_obj.read_required)
    sock_obj.read_required = sep
    suspend_socket(sock_obj)
    if sock_obj.connected then
        return socket_core.read_line(sock_obj.buffer, sock_obj.buffer_pool, sep)
    else
        return false, socket_core.read_all(sock_obj.buffer, sock_obj.buffer_pool)
    end
end

---
---@param socket_id
function socket.block(socket_id)
    local sock_obj = socket_pool[socket_id]
    if not sock_obj or not sock_obj.connected then
        return false
    end
    assert(not sock_obj.read_required)
    sock_obj.read_required = 0
    suspend_socket(sock_obj)
    return sock_obj.connected
end

socket.write = assert(socket_core.send)
socket.lwrite = assert(socket_core.lsend) -- send low priority
socket.header = assert(socket_core.header)

---
---@param socket_id
function socket.invalid(socket_id)
    return socket_pool[socket_id] == nil
end

---
---@param socket_id
function socket.disconnected(socket_id)
    local sock_obj = socket_pool[socket_id]
    if sock_obj then
        return not (sock_obj.connected or sock_obj.connecting)
    end
end

---
---@param host string
---@param port number
---@param backlog number
function socket.listen(host, port, backlog)
    if port == nil then
        host, port = string.match(host, "([^:]+):(.+)$")
        port = tonumber(port)
    end
    return socket_core.listen(host, port, backlog)
end

---
---@param socket_id
function socket.lock(socket_id)
    local sock_obj = socket_pool[socket_id]
    assert(sock_obj)
    local lock_set = sock_obj.lock_set
    if not lock_set then
        lock_set = {}
        sock_obj.lock_set = lock_set
    end
    if #lock_set == 0 then
        lock_set[1] = true
    else
        local current_thread = coroutine.running()
        table.insert(lock_set, current_thread)
        skynet.wait(current_thread)
    end
end

---
---@param socket_id
function socket.unlock(socket_id)
    local sock_obj = socket_pool[socket_id]
    assert(sock_obj)
    local lock_set = assert(sock_obj.lock_set)
    table.remove(lock_set, 1)
    local thread = lock_set[1]
    if thread then
        skynet.wakeup(thread)
    end
end

---
--- use to forward socket id to other service
--- you must call socket.start(socket_id) later in other service
---@param socket_id
function socket.abandon(socket_id)
    local sock_obj = socket_pool[socket_id]
    if sock_obj then
        sock_obj.connected = false
        wakeup_socket(sock_obj)
        socket_pool[socket_id] = nil
    end
end

---
--- set socket buffer limit
---@param socket_id number
---@param limit number socket buffer limit
function socket.limit(socket_id, limit)
    local sock_obj = assert(socket_pool[socket_id])
    sock_obj.buffer_limit = limit
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
    local socket_id = socket_core.udp(host, port)
    create_udp_object(socket_id, callback)
    return socket_id
end

function socket.udp_connect(socket_id, addr, port, callback)
    local sock_obj = socket_pool[socket_id]
    if sock_obj then
        assert(sock_obj.protocol == "UDP")
        if callback then
            sock_obj.callback = callback
        end
    else
        create_udp_object(socket_id, callback)
    end
    socket_core.udp_connect(socket_id, addr, port)
end

socket.sendto = assert(socket_core.udp_send)
socket.udp_address = assert(socket_core.udp_address)
socket.netstat = assert(socket_core.info)

function socket.warning(socket_id, callback)
    local sock_obj = socket_pool[socket_id]
    assert(sock_obj)
    sock_obj.on_warning = callback
end

function socket.onclose(socket_id, callback)
    local sock_obj = socket_pool[socket_id]
    assert(sock_obj)
    sock_obj.on_close = callback
end

-- skynet socket message handle

do
    local socket_message = {}

    socket_message[socket.SKYNET_SOCKET_EVENT_DATA] = function(socket_id, size, data)
        local sock_obj = socket_pool[socket_id]
        if sock_obj == nil then
            skynet.log("socket: drop package from " .. socket_id)
            socket_core.drop(data, size)
            return
        end

        -- push data to socket buffer, will get a free buffer_node from pool, and then put the data/size in it.
        local sz = socket_core.push_buffer(sock_obj.buffer, sock_obj.buffer_pool, data, size)
        local rr = sock_obj.read_required
        local rrt = type(rr)
        if rrt == "number" then
            -- read size
            if sz >= rr then
                sock_obj.read_required = nil
                if sz > BUFFER_LIMIT then
                    pause_socket(sock_obj, sz)
                end
                wakeup_socket(sock_obj)
            end
        else
            -- check socket buffer limit
            if sock_obj.buffer_limit and sz > sock_obj.buffer_limit then
                skynet.log(string.format("socket buffer overflow: fd=%d size=%d", socket_id, sz))
                socket_core.close(socket_id)
                return
            end
            if rrt == "string" then
                -- read line
                if socket_core.read_line(sock_obj.buffer, nil, rr) then
                    sock_obj.read_required = nil
                    if sz > BUFFER_LIMIT then
                        pause_socket(sock_obj, sz)
                    end
                    wakeup_socket(sock_obj)
                end
            elseif sz > BUFFER_LIMIT and not sock_obj.is_pause then
                pause_socket(sock_obj, sz)
            end
        end
    end

    socket_message[socket.SKYNET_SOCKET_EVENT_CONNECT] = function(socket_id, _, addr)
        local sock_obj = socket_pool[socket_id]
        if sock_obj == nil then
            return
        end
        -- log remote addr
        if not sock_obj.connected then
            -- resume may also post connect message
            sock_obj.connected = true
            wakeup_socket(sock_obj)
        end
    end

    socket_message[socket.SKYNET_SOCKET_EVENT_CLOSE] = function(socket_id)
        local sock_obj = socket_pool[socket_id]
        if sock_obj == nil then
            socket_core.close(socket_id)
            return
        end
        sock_obj.connected = false
        wakeup_socket(sock_obj)
        if sock_obj.on_close then
            sock_obj.on_close(socket_id)
        end
    end

    -- SKYNET_SOCKET_EVENT_ACCEPT = 4
    socket_message[4] = function(socket_id, newid, addr)
        local sock_obj = socket_pool[socket_id]
        if sock_obj == nil then
            socket_core.close(newid)
            return
        end
        sock_obj.callback(newid, addr)
    end

    socket_message[socket.SKYNET_SOCKET_EVENT_ERROR] = function(socket_id, _, err)
        local sock_obj = socket_pool[socket_id]
        if sock_obj == nil then
            socket_core.shutdown(socket_id)
            skynet.log("socket: error on unknown", socket_id, err)
            return
        end
        if sock_obj.callback then
            skynet.log("socket: accpet error:", err)
            return
        end
        if sock_obj.connected then
            skynet.log("socket: error on", socket_id, err)
        elseif sock_obj.connecting then
            sock_obj.connecting = err
        end
        sock_obj.connected = false
        socket_core.shutdown(socket_id)

        wakeup_socket(sock_obj)
    end

    socket_message[socket.SKYNET_SOCKET_EVENT_UDP] = function(socket_id, size, data, address)
        local sock_obj = socket_pool[socket_id]
        if sock_obj == nil or sock_obj.callback == nil then
            skynet.log("socket: drop udp package from " .. socket_id)
            socket_core.drop(data, size)
            return
        end
        local str = skynet.tostring(data, size)
        skynet_core.trash(data, size)
        sock_obj.callback(str, address)
    end

    local function default_warning(socket_id, size)
        local sock_obj = socket_pool[socket_id]
        if not sock_obj then
            return
        end
        skynet.log(string.format("WARNING: %d K bytes need to send out (fd = %d)", size, socket_id))
    end

    socket_message[socket.SKYNET_SOCKET_EVENT_WARNING] = function(socket_id, size)
        local sock_obj = socket_pool[socket_id]
        if sock_obj then
            local warning = sock_obj.on_warning or default_warning
            warning(socket_id, size)
        end
    end

    skynet.register_svc_msg_handler({
        msg_type_name = "socket",
        msg_type = skynet.SERVICE_MSG_TYPE_SOCKET, -- SERVICE_MSG_TYPE_SOCKET = 6
        unpack = socket_core.unpack,
        dispatch = function(_, _, type, ...)
            socket_message[type](...)
        end
    })
end

return socket
