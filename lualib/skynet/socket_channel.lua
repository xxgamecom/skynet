local skynet = require "skynet"
local socket = require "skynet.socket"
local socket_core = require "skynet.socket.core"

-- channel support auto reconnect , and capture socket error in request/response transaction
-- { host = "", port = , auth = function(so) , response = function(so) session, data }

local socket_channel = {}
local channel = {}
local channel_socket = {}
local channel_meta = { __index = channel }
local channel_socket_meta = {
    __index = channel_socket,
    __gc = function(cs)
        local fd = cs[1]
        cs[1] = false
        if fd then
            socket.shutdown(fd)
        end
    end
}

local socket_error = setmetatable({}, { __tostring = function()
    return "[Error: socket]"
end })    -- alias for error object
socket_channel.error = socket_error

function socket_channel.channel(desc)
    local c = {
        __host = assert(desc.host),
        __port = assert(desc.port),
        __backup = desc.backup,
        __auth = desc.auth,
        __response = desc.response, -- It's for session mode
        __request = {}, -- request seq { response func or session }	-- It's for order mode
        __thread = {}, -- coroutine seq or session->coroutine map
        __result = {}, -- response result { coroutine -> result }
        __result_data = {},
        __connecting = {},
        __sock = false,
        __closed = false,
        __authcoroutine = false,
        __nodelay = desc.nodelay,
        __overload_notify = desc.overload,
        __overload = false,
    }

    return setmetatable(c, channel_meta)
end

local function close_channel_socket(self)
    if self.__sock then
        local so = self.__sock
        self.__sock = false
        -- never raise error
        pcall(socket.close, so[1])
    end
end

local function wakeup_all(self, errmsg)
    if self.__response then
        for k, thread in pairs(self.__thread) do
            self.__thread[k] = nil
            self.__result[thread] = socket_error
            self.__result_data[thread] = errmsg
            skynet.wakeup(thread)
        end
    else
        for i = 1, #self.__request do
            self.__request[i] = nil
        end
        for i = 1, #self.__thread do
            local thread = self.__thread[i]
            self.__thread[i] = nil
            if thread then
                -- ignore the close signal
                self.__result[thread] = socket_error
                self.__result_data[thread] = errmsg
                skynet.wakeup(thread)
            end
        end
    end
end

local function dispatch_by_session(self)
    local response = self.__response
    -- response() return session
    while self.__sock do
        local ok, session, result_ok, result_data, padding = pcall(response, self.__sock)
        if ok and session then
            local thread = self.__thread[session]
            if thread then
                if padding and result_ok then
                    -- If padding is true, append result_data to a table (self.__result_data[thread])
                    local result = self.__result_data[thread] or {}
                    self.__result_data[thread] = result
                    table.insert(result, result_data)
                else
                    self.__thread[session] = nil
                    self.__result[thread] = result_ok
                    if result_ok and self.__result_data[thread] then
                        table.insert(self.__result_data[thread], result_data)
                    else
                        self.__result_data[thread] = result_data
                    end
                    skynet.wakeup(thread)
                end
            else
                self.__thread[session] = nil
                skynet.log_error("socket: unknown session :", session)
            end
        else
            close_channel_socket(self)
            local errormsg
            if session ~= socket_error then
                errormsg = session
            end
            wakeup_all(self, errormsg)
        end
    end
end

local function pop_response(self)
    while true do
        local func, thread = table.remove(self.__request, 1), table.remove(self.__thread, 1)
        if func then
            return func, thread
        end
        self.__wait_response = coroutine.running()
        skynet.wait(self.__wait_response)
    end
end

-- on close callback
local function autoclose_cb(self, fd)
    local sock = self.__sock
    if self.__wait_response and sock and sock[1] == fd then
        -- closed by peer
        skynet.log_info("socket closed by peer : ", self.__host, self.__port)
        close_channel_socket(self)
    end
end

local function push_response(self, response, thread)
    if self.__response then
        -- response is session
        self.__thread[response] = thread
    else
        -- response is a function, push it to __request
        table.insert(self.__request, response)
        table.insert(self.__thread, thread)
        if self.__wait_response then
            skynet.wakeup(self.__wait_response)
            self.__wait_response = nil
        end
    end
end

local function get_response(func, sock)
    local result_ok, result_data, padding = func(sock)
    if result_ok and padding then
        local result = { result_data }
        local index = 2
        repeat
            result_ok, result_data, padding = func(sock)
            if not result_ok then
                return result_ok, result_data
            end
            result[index] = result_data
            index = index + 1
        until not padding
        return true, result
    else
        return result_ok, result_data
    end
end

local function dispatch_by_order(self)
    while self.__sock do
        local func, thread = pop_response(self)
        if not thread then
            -- close signal
            wakeup_all(self, "channel_closed")
            break
        end
        local sock = self.__sock
        if not sock then
            -- closed by peer
            self.__result[thread] = socket_error
            skynet.wakeup(thread)
            wakeup_all(self)
            break
        end
        local ok, result_ok, result_data = pcall(get_response, func, sock)
        if ok then
            self.__result[thread] = result_ok
            if result_ok and self.__result_data[thread] then
                table.insert(self.__result_data[thread], result_data)
            else
                self.__result_data[thread] = result_data
            end
            skynet.wakeup(thread)
        else
            close_channel_socket(self)
            local errmsg
            if result_ok ~= socket_error then
                errmsg = result_ok
            end
            self.__result[thread] = socket_error
            self.__result_data[thread] = errmsg
            skynet.wakeup(thread)
            wakeup_all(self, errmsg)
        end
    end
end

local function dispatch_function(self)
    if self.__response then
        return dispatch_by_session
    else
        socket.onclose(self.__sock[1], function(fd)
            autoclose_cb(self, fd)
        end)
        return dispatch_by_order
    end
end

local function term_dispatch_thread(self)
    if not self.__response and self.__dispatch_thread then
        -- dispatch by order, send close signal to dispatch thread
        push_response(self, true, false)    -- (true, false) is close signal
    end
end

local function connect_once(self)
    if self.__closed then
        return false
    end

    local addr_list = {}
    local addr_set = {}

    local function _add_backup()
        if self.__backup then
            for _, addr in ipairs(self.__backup) do
                local host, port
                if type(addr) == "table" then
                    host, port = addr.host, addr.port
                else
                    host = addr
                    port = self.__port
                end

                -- don't add the same host
                local hostkey = host .. ":" .. port
                if not addr_set[hostkey] then
                    addr_set[hostkey] = true
                    table.insert(addr_list, { host = host, port = port })
                end
            end
        end
    end

    local function _next_addr()
        local addr = table.remove(addr_list, 1)
        if addr then
            skynet.log_info("socket: connect to backup host", addr.host, addr.port)
        end
        return addr
    end

    local function _connect_once(self, addr)
        local fd, err = socket.open_tcp_client(addr.host, addr.port)
        if not fd then
            -- try next one
            addr = _next_addr()
            if addr == nil then
                return false, err
            end
            return _connect_once(self, addr)
        end

        self.__host = addr.host
        self.__port = addr.port

        assert(not self.__sock and not self.__authcoroutine)
        -- term current dispatch thread (send a signal)
        term_dispatch_thread(self)

        if self.__nodelay then
            socket_core.nodelay(fd)
        end

        -- register overload warning

        local overload = self.__overload_notify
        if overload then
            local function overload_trigger(id, size)
                if id == self.__sock[1] then
                    if size == 0 then
                        if self.__overload then
                            self.__overload = false
                            overload(false)
                        end
                    else
                        if not self.__overload then
                            self.__overload = true
                            overload(true)
                        else
                            skynet.log_warn(string.format("WARNING: %d K bytes need to send out (fd = %d %s:%s)", size, id, self.__host, self.__port))
                        end
                    end
                end
            end

            skynet.fork(overload_trigger, fd, 0)
            socket.warning(fd, overload_trigger)
        end

        while self.__dispatch_thread do
            -- wait for dispatch thread exit
            skynet.yield()
        end

        self.__sock = setmetatable({ fd }, channel_socket_meta)
        self.__dispatch_thread = skynet.fork(function()
            pcall(dispatch_function(self), self)
            -- clear dispatch_thread
            self.__dispatch_thread = nil
        end)

        if self.__auth then
            self.__authcoroutine = coroutine.running()
            local ok, message = pcall(self.__auth, self)
            if not ok then
                close_channel_socket(self)
                if message ~= socket_error then
                    self.__authcoroutine = false
                    skynet.log_error("socket: auth failed", message)
                end
            end
            self.__authcoroutine = false
            if ok then
                if not self.__sock then
                    -- auth may change host, so connect again
                    return connect_once(self)
                end
                -- auth succ, go through
            else
                -- auth failed, try next addr
                _add_backup()    -- auth may add new backup hosts
                addr = _next_addr()
                if addr == nil then
                    return false, "no more backup host"
                end
                return _connect_once(self, addr)
            end
        end

        return true
    end

    _add_backup()
    return _connect_once(self, { host = self.__host, port = self.__port })
end

local function try_connect(self, once)
    local t = 0
    while not self.__closed do
        local ok, err = connect_once(self)
        if ok then
            if not once then
                skynet.log_info("socket: connect to", self.__host, self.__port)
            end
            return
        elseif once then
            return err
        else
            skynet.log_info("socket: connect", err)
        end
        if t > 1000 then
            skynet.log_info("socket: try to reconnect", self.__host, self.__port)
            skynet.sleep(t)
            t = 0
        else
            skynet.sleep(t)
        end
        t = t + 100
    end
end

local function check_connection(self)
    if self.__sock then
        if socket.disconnected(self.__sock[1]) then
            -- closed by peer
            skynet.log_info("socket: disconnect detected ", self.__host, self.__port)
            close_channel_socket(self)
            return
        end
        local authco = self.__authcoroutine
        if not authco then
            return true
        end
        if authco == coroutine.running() then
            -- authing
            return true
        end
    end
    if self.__closed then
        return false
    end
end

local function block_connect(self, once)
    local r = check_connection(self)
    if r ~= nil then
        return r
    end
    local err

    if #self.__connecting > 0 then
        -- connecting in other coroutine
        local thread = coroutine.running()
        table.insert(self.__connecting, thread)
        skynet.wait(thread)
    else
        self.__connecting[1] = true
        err = try_connect(self, once)
        self.__connecting[1] = nil
        for i = 2, #self.__connecting do
            local thread = self.__connecting[i]
            self.__connecting[i] = nil
            skynet.wakeup(thread)
        end
    end

    r = check_connection(self)
    if r == nil then
        skynet.log_error(string.format("Connect to %s:%d failed (%s)", self.__host, self.__port, err))
        error(socket_error)
    else
        return r
    end
end

function channel:connect(once)
    self.__closed = false
    return block_connect(self, once)
end

local function wait_for_response(self, response)
    local thread = coroutine.running()
    push_response(self, response, thread)
    skynet.wait(thread)

    local result = self.__result[thread]
    self.__result[thread] = nil
    local result_data = self.__result_data[thread]
    self.__result_data[thread] = nil

    if result == socket_error then
        if result_data then
            error(result_data)
        else
            error(socket_error)
        end
    else
        assert(result, result_data)
        return result_data
    end
end

local socket_send = socket.send
local socket_send_low = socket.send_low

local function sock_err(self)
    close_channel_socket(self)
    wakeup_all(self)
    error(socket_error)
end

function channel:request(request, response, padding)
    assert(block_connect(self, true))    -- connect once
    local fd = self.__sock[1]

    if padding then
        -- padding may be a table, to support multi part request
        -- multi part request use low priority socket write
        -- now socket_send_low returns as socket_send
        if not socket_send_low(fd, request) then
            sock_err(self)
        end
        for _, v in ipairs(padding) do
            if not socket_send_low(fd, v) then
                sock_err(self)
            end
        end
    else
        if not socket_send(fd, request) then
            sock_err(self)
        end
    end

    if response == nil then
        -- no response
        return
    end

    return wait_for_response(self, response)
end

function channel:response(response)
    assert(block_connect(self))

    return wait_for_response(self, response)
end

function channel:close()
    if not self.__closed then
        term_dispatch_thread(self)
        self.__closed = true
        close_channel_socket(self)
    end
end

function channel:changehost(host, port)
    self.__host = host
    if port then
        self.__port = port
    end
    if not self.__closed then
        close_channel_socket(self)
    end
end

function channel:changebackup(backup)
    self.__backup = backup
end

channel_meta.__gc = channel.close

local function wrapper_socket_function(f)
    return function(self, ...)
        local result = f(self[1], ...)
        if not result then
            error(socket_error)
        else
            return result
        end
    end
end

channel_socket.read = wrapper_socket_function(socket.read)
channel_socket.read_line = wrapper_socket_function(socket.read_line)

return socket_channel
