--
-- socket api wrapper
--
--

local skynet = require "skynet"
local skynet_core = require "skynet.core"
local socket_core = require "skynet.socket.core"

local pairs = pairs
local assert = assert
local tonumber = tonumber
local table_insert = table.insert
local table_remove = table.remove

local BUFFER_LIMIT = 128 * 1024

local socket = {
    -- define socket event
    SKYNET_SOCKET_EVENT_DATA = 1,
    SKYNET_SOCKET_EVENT_CONNECT = 2,
    SKYNET_SOCKET_EVENT_CLOSE = 3,
    SKYNET_SOCKET_EVENT_ACCEPT = 4,
    SKYNET_SOCKET_EVENT_ERROR = 5,
    SKYNET_SOCKET_EVENT_UDP = 6,
    SKYNET_SOCKET_EVENT_WARNING = 7,
}

-- store all socket object
local socket_object_pool = setmetatable({}, { __gc = function(p)
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
        skynet.log_info(string.format("Pause socket (%d) size : %d", sock_obj.socket_id, size))
    else
        skynet.log_info(string.format("Pause socket (%d)", sock_obj.socket_id))
    end
    socket_core.pause(sock_obj.socket_id)
    sock_obj.is_pause = true
    skynet.yield()    -- there are subsequent socket messages in mqueue, maybe.
end

---
--- suspend the socket thread
---@param sock_obj table socket object
local function suspend_socket(sock_obj)
    --
    assert(not sock_obj.thread)
    sock_obj.thread = coroutine.running()

    --
    if sock_obj.is_pause then
        skynet.log_info(string.format("Resume socket (%d)", sock_obj.socket_id))
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


local function create_tcp_socket_object(socket_id, callback)
    local new_buffer
    if callback == nil then
        new_buffer = socket_core.new_buffer()
    end

    assert(not socket_object_pool[socket_id], "socket is not closed")
    local sock_obj = {
        socket_id = socket_id,
        buffer = new_buffer,
        buffer_pool = new_buffer and {},
        connected = false, -- connected flag
        connecting = true, -- connecting flag
        read_required = false, --
        thread = false, -- socket thread, false means
        callback = callback, --
        socket_type = "TCP", -- socket ip proto type
    }
    socket_object_pool[socket_id] = sock_obj

    return sock_obj
end

---
---@param socket_id number socket logic id
---@param callback function callback function
---@return number, string socket_id, error msg
local function connect(socket_id, callback)
    -- create socket object
    local sock_obj = create_tcp_socket_object(socket_id, callback)

    --
    suspend_socket(sock_obj)

    -- reset connecting status, get connect error message
    local err = sock_obj.connecting
    sock_obj.connecting = nil

    -- connect success
    if sock_obj.connected then
        return socket_id
    end

    -- connect failed
    socket_object_pool[socket_id] = nil
    return nil, err
end

-- ----------------------------------
-- TCP
-- ----------------------------------

---
--- open a tcp server
---@param local_addr string local ip or host uri (`ip:port` string)
---@param local_port number local port or nil (when local_addr is a host uri)
---@param backlog number
---@return number socket logic id
function socket.open_tcp_server(local_addr, local_port, backlog)
    -- parse host uri
    if local_port == nil then
        local_addr, local_port = string.match(local_addr, "([^:]+):(.+)$")
        local_port = tonumber(local_port)
    end

    --
    local socket_id = socket_core.listen(local_addr, local_port, backlog)
    skynet.log_debug("Open tcp server", local_addr, local_port, backlog, socket_id)

    return socket_id
end

---
--- open a tcp client (connect remote tcp server), will block entil connect success or failed.
---@param remote_addr string remote server addr
---@param remote_port number remote server port
---@return number, string socket_id, error msg
function socket.open_tcp_client(remote_addr, remote_port)
    --
    local socket_id = socket_core.connect(remote_addr, remote_port)
    skynet.log_debug("Open tcp client", remote_addr, remote_port, socket_id)

    --
    return connect(socket_id)
end

---
--- start a socket.
--- - for tcp server, must be listen before;
--- - for
---@param socket_id number socket logic id
---@param func function callback function
---@return number, string socket_id, error msg
function socket.start(socket_id, func)
    --
    socket_core.start(socket_id)
    skynet.log_debug("Start socket", socket_id)

    --
    return connect(socket_id, func)
end

---
--- puase a socket.
---@param socket_id number socket logic id
function socket.pause(socket_id)
    local sock_obj = socket_object_pool[socket_id]
    if sock_obj == nil or sock_obj.is_pause then
        return
    end

    pause_socket(sock_obj)
    skynet.log_debug("Pause socket", socket_id)
end

---
---@param socket_id
function socket.close(socket_id)
    local sock_obj = socket_object_pool[socket_id]
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
    socket_object_pool[socket_id] = nil

    skynet.log_debug("Close socket", socket_id)
end

---
---@param socket_id
function socket.shutdown(socket_id)
    local sock_obj = socket_object_pool[socket_id]
    if sock_obj then
        -- the framework would send SKYNET_SOCKET_EVENT_CLOSE , need close(socket_id) later
        socket_core.shutdown(socket_id)
    end

    skynet.log_debug("Shutdown socket", socket_id)
end

---
---  os fd redirection
---@param os_fd
function socket.bind_os_fd(os_fd)
    --
    local socket_id = socket_core.bind_os_fd(os_fd)
    skynet.log_debug("Redirect os fd", os_fd)

    --
    return connect(socket_id)
end

---
--- stdin redirection
function socket.stdin()
    return socket.bind_os_fd(0)
end

---
---
---@param socket_id number socket logic id
---@param sz number read size, nil means read some bytes
---@return boolean, string result and data
function socket.read(socket_id, sz)
    local sock_obj = socket_object_pool[socket_id]
    assert(sock_obj)

    --
    if sz == nil then
        -- read some bytes
        local ret = socket_core.read_all(sock_obj.buffer, sock_obj.buffer_pool)
        if ret ~= "" then
            return ret
        end

        --
        if not sock_obj.connected then
            return false, ret
        end

        --
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
---@param socket_id number socket logic id
function socket.read_all(socket_id)
    --
    local sock_obj = socket_object_pool[socket_id]
    assert(sock_obj)

    --
    if not sock_obj.connected then
        local ret = socket_core.read_all(sock_obj.buffer, sock_obj.buffer_pool)
        return ret ~= "" and ret
    end

    --
    assert(not sock_obj.read_required)
    sock_obj.read_required = true
    suspend_socket(sock_obj)
    assert(sock_obj.connected == false)
    return socket_core.read_all(sock_obj.buffer, sock_obj.buffer_pool)
end

---
---
---@param socket_id number socket logic id
---@param sep string
function socket.read_line(socket_id, sep)
    sep = sep or "\n"
    local sock_obj = socket_object_pool[socket_id]
    assert(sock_obj)

    --
    local ret = socket_core.read_line(sock_obj.buffer, sock_obj.buffer_pool, sep)
    if ret then
        return ret
    end

    --
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

function socket.write(...)
    return socket_core.send(...)
end

function socket.lwrite(...)
    return socket_core.lsend(...)
end

function socket.header(...)
    return socket_core.header(...)
end

---
---@param socket_id
function socket.block(socket_id)
    local sock_obj = socket_object_pool[socket_id]
    if not sock_obj or not sock_obj.connected then
        return false
    end
    assert(not sock_obj.read_required)
    sock_obj.read_required = 0
    suspend_socket(sock_obj)
    return sock_obj.connected
end

---
---@param socket_id
function socket.invalid(socket_id)
    return socket_object_pool[socket_id] == nil
end

---
---@param socket_id
function socket.disconnected(socket_id)
    local sock_obj = socket_object_pool[socket_id]
    if sock_obj then
        return not (sock_obj.connected or sock_obj.connecting)
    end
end

---
---@param socket_id
function socket.lock(socket_id)
    local sock_obj = socket_object_pool[socket_id]
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
        table_insert(lock_set, current_thread)
        skynet.wait(current_thread)
    end
end

---
---@param socket_id
function socket.unlock(socket_id)
    local sock_obj = socket_object_pool[socket_id]
    assert(sock_obj)

    local lock_set = assert(sock_obj.lock_set)
    table_remove(lock_set, 1)
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
    local sock_obj = socket_object_pool[socket_id]
    if sock_obj then
        sock_obj.connected = false
        wakeup_socket(sock_obj)
        socket_object_pool[socket_id] = nil
    end
end

---
--- set socket buffer limit
---@param socket_id number
---@param limit number socket buffer limit
function socket.limit(socket_id, limit)
    local sock_obj = assert(socket_object_pool[socket_id])
    sock_obj.buffer_limit = limit
end

-- ----------------------------------
-- UDP
-- ----------------------------------

local function create_udp_socket_object(socket_id, callback)
    assert(not socket_object_pool[socket_id], "socket is not closed")
    local sock_obj = {
        socket_id = socket_id,
        connected = true,
        socket_type = "UDP",
        callback = callback,
    }
    socket_object_pool[socket_id] = sock_obj

    return sock_obj
end

---
--- create an udp socket
---@param callback function
---@param local_ip string
---@param local_port number
function socket.udp_socket(callback, local_ip, local_port)
    local socket_id = socket_core.udp_socket(local_ip, local_port)
    create_udp_socket_object(socket_id, callback)
    return socket_id
end

---
--- create an udp client
---@param socket_id number socket logic id, @see socket.udp_socket()
---@param remote_ip string
---@param remote_port number
---@param callback function
function socket.udp_connect(socket_id, remote_ip, remote_port, callback)
    local sock_obj = socket_object_pool[socket_id]
    if sock_obj then
        assert(sock_obj.socket_type == "UDP")
        if callback then
            sock_obj.callback = callback
        end
    else
        create_udp_socket_object(socket_id, callback)
    end
    socket_core.udp_connect(socket_id, remote_ip, remote_port)
end

function socket.sendto(...)
    return socket_core.udp_send(...)
end

function socket.udp_address(...)
    return socket_core.udp_address(...)
end

-- ----------------------------------
--
-- ----------------------------------

function socket.netstat()
    return socket_core.info()
end

function socket.warning(socket_id, callback)
    local sock_obj = socket_object_pool[socket_id]
    assert(sock_obj)
    sock_obj.on_warning = callback
end

function socket.onclose(socket_id, callback)
    local sock_obj = socket_object_pool[socket_id]
    assert(sock_obj)
    sock_obj.on_close = callback
end

-- ----------------------------------
-- skynet socket message handle
-- ----------------------------------

do
    local socket_message = {}

    socket_message[socket.SKYNET_SOCKET_EVENT_DATA] = function(socket_id, size, data)
        local sock_obj = socket_object_pool[socket_id]
        if sock_obj == nil then
            skynet.log_error("socket: drop package from", socket_id)
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
                skynet.log_error(string.format("socket buffer overflow: fd=%d size=%d", socket_id, sz))
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
        local sock_obj = socket_object_pool[socket_id]
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
        local sock_obj = socket_object_pool[socket_id]
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
        local sock_obj = socket_object_pool[socket_id]
        if sock_obj == nil then
            socket_core.close(newid)
            return
        end
        sock_obj.callback(newid, addr)
    end

    socket_message[socket.SKYNET_SOCKET_EVENT_ERROR] = function(socket_id, _, err)
        local sock_obj = socket_object_pool[socket_id]
        if sock_obj == nil then
            socket_core.shutdown(socket_id)
            skynet.log_error("socket: error on unknown", socket_id, err)
            return
        end
        if sock_obj.callback then
            skynet.log_error("socket: accpet error:", err)
            return
        end
        if sock_obj.connected then
            skynet.log_error("socket: error on", socket_id, err)
        elseif sock_obj.connecting then
            sock_obj.connecting = err
        end
        sock_obj.connected = false
        socket_core.shutdown(socket_id)

        wakeup_socket(sock_obj)
    end

    socket_message[socket.SKYNET_SOCKET_EVENT_UDP] = function(socket_id, size, data, address)
        local sock_obj = socket_object_pool[socket_id]
        if sock_obj == nil or sock_obj.callback == nil then
            skynet.log_warn("socket: drop udp package from " .. socket_id)
            socket_core.drop(data, size)
            return
        end
        local str = skynet.tostring(data, size)
        skynet_core.trash(data, size)
        sock_obj.callback(str, address)
    end

    local function default_warning(socket_id, size)
        local sock_obj = socket_object_pool[socket_id]
        if not sock_obj then
            return
        end
        skynet.log_warn(string.format("WARNING: %d K bytes need to send out (fd = %d)", size, socket_id))
    end

    socket_message[socket.SKYNET_SOCKET_EVENT_WARNING] = function(socket_id, size)
        local sock_obj = socket_object_pool[socket_id]
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
