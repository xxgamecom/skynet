local skynet_core = require "skynet.core"

local profile = require "skynet.profile"
local profile_resume = profile.resume
local profile_yield = profile.yield

local tostring = tostring
local coroutine = coroutine
local assert = assert
local pairs = pairs
local ipairs = ipairs
local pcall = pcall
local table = table
local table_remove = table.remove
local table_insert = table.insert
local traceback = debug.traceback

-- ------------------------------------------------------
-- coroutine functions
--
-- thread_create
-- thread_resume
-- thread_yield
-- thread_running
-- ------------------------------------------------------

local current_thread = nil -- record current running thread

local thread_create = coroutine.create
local thread_resume = function(thread, ...)
    current_thread = thread
    return profile_resume(thread, ...)
end
local thread_yield = profile_yield
local thread_running = coroutine.running

-- ------------------------------------------------------
--
-- ------------------------------------------------------

local init_thread = nil

-- skynet service message handler map
local svc_msg_handlers = {}

local skynet = {
    SERVICE_MSG_TYPE_TEXT = 0,
    SERVICE_MSG_TYPE_RESPONSE = 1,
    SERVICE_MSG_TYPE_MULTICAST = 2,
    SERVICE_MSG_TYPE_CLIENT = 3,
    SERVICE_MSG_TYPE_SYSTEM = 4,
    SERVICE_MSG_TYPE_SOCKET = 5,
    SERVICE_MSG_TYPE_ERROR = 6,
    SERVICE_MSG_TYPE_QUEUE = 7, -- used in deprecated mqueue, use skynet.queue instead
    SERVICE_MSG_TYPE_DEBUG = 8,
    SERVICE_MSG_TYPE_LUA = 9,
    SERVICE_MSG_TYPE_SNAX = 10,
    SERVICE_MSG_TYPE_TRACE = 11, -- use for debug trace
}

-- code cache
skynet.cache = require "skynet.codecache"

---
--- register skynet service message handler
--- @param svc_msg_handler table the service messsage handler
function skynet.register_protocol(svc_msg_handler)
    local msg_type_name = svc_msg_handler.msg_type_name
    local msg_type = svc_msg_handler.msg_type
    assert(svc_msg_handlers[msg_type_name] == nil and svc_msg_handlers[msg_type] == nil)
    assert(type(msg_type_name) == "string" and type(msg_type) == "number" and msg_type >= 0 and msg_type <= 255)
    svc_msg_handlers[msg_type_name] = svc_msg_handler
    svc_msg_handlers[msg_type] = svc_msg_handler
end

--
local session_thread_map = {}           -- key: session_id, value: coroutine
local thread_session_map = {}           -- key: coroutine, value: session_id
local thread_svc_handle_map = {}        -- key: coroutine, value: service handle
local thread_trace_tag_map = {}         -- key: coroutine, value: trace tag (format: (":%08x-%d", self_svc_handle, trace_id))

--
local unresponse = {}

local wakeup_queue = {}
local sleep_session = {}

local watching_session = {} --
local fork_queue = {} -- fork coroutine exec queue

-- thread suspend function
local suspend


----- monitor exit

local error_queue = {}

local function dispatch_error_queue()
    local session_id = table_remove(error_queue, 1)
    if session_id then
        local thread = session_thread_map[session_id]
        session_thread_map[session_id] = nil
        return suspend(thread, thread_resume(thread, false))
    end
end

local function _error_dispatch(error_session_id, error_src_svc_handle)
    skynet.ignoreret()    -- don't return for error
    if error_session_id == 0 then
        -- error_src_svc_handle is down, clear unreponse set
        for res, address in pairs(unresponse) do
            if error_src_svc_handle == address then
                unresponse[res] = nil
            end
        end
        for session_id, svc_handle in pairs(watching_session) do
            if svc_handle == error_src_svc_handle then
                table_insert(error_queue, session_id)
            end
        end
    else
        -- capture an error for error_session_id
        if watching_session[error_session_id] then
            table_insert(error_queue, error_session_id)
        end
    end
end

-- ------------------------------------------------------
-- thread pool
-- ------------------------------------------------------

local thread_pool = setmetatable({}, { __mode = "kv" })

---
--- create thread from thread pool
--- @param func function thread function
local function co_create(func)
    -- get a thread from pool
    local thread = table_remove(thread_pool)

    -- no more thread in pool, need create
    if thread == nil then
        thread = thread_create(function(...)
            --
            func(...)

            --
            while true do
                -- trace thread info
                local session_id = thread_session_map[thread]
                if session_id and session_id ~= 0 then
                    local source = debug.getinfo(func, "S")
                    skynet.log(string.format("Maybe forgot response session %s from %s : %s:%d",
                        session_id,
                        skynet.to_address(thread_svc_handle_map[thread]),
                        source.source,
                        source.linedefined)
                    )
                end

                -- coroutine exit
                local tag = thread_trace_tag_map[thread]
                if tag ~= nil then
                    if tag then
                        skynet_core.trace(tag, "end")
                    end
                    thread_trace_tag_map[thread] = nil
                end
                local address = thread_svc_handle_map[thread]
                if address then
                    thread_session_map[thread] = nil
                    thread_svc_handle_map[thread] = nil
                end

                -- recycle thread into pool
                func = nil
                thread_pool[#thread_pool + 1] = thread
                -- recv new main function
                func = thread_yield("SUSPEND")
                func(thread_yield())
            end
        end)
    else
        -- pass the main function to coroutine, and restore running thread
        local running = current_thread
        thread_resume(thread, func)
        current_thread = running
    end
    return thread
end

local function dispatch_wakeup()
    local token = table_remove(wakeup_queue, 1)
    if token then
        local session_id = sleep_session[token]
        if session_id then
            local thread = session_thread_map[session_id]
            local tag = thread_trace_tag_map[thread]
            if tag then
                skynet_core.trace(tag, "resume")
            end
            session_thread_map[session_id] = "BREAK"
            return suspend(thread, thread_resume(thread, false, "BREAK"))
        end
    end
end

-- suspend is local function
function suspend(thread, result, command)
    if not result then
        local session_id = thread_session_map[thread]
        if session_id then
            -- coroutine may fork by others (session_id is nil)
            local addr = thread_svc_handle_map[thread]
            if session_id ~= 0 then
                -- only call response error
                local tag = thread_trace_tag_map[thread]
                if tag then
                    skynet_core.trace(tag, "error")
                end
                skynet_core.send(addr, skynet.SERVICE_MSG_TYPE_ERROR, session_id, "")
            end
            thread_session_map[thread] = nil
        end
        thread_svc_handle_map[thread] = nil
        thread_trace_tag_map[thread] = nil
        skynet.fork(function()
        end)    -- trigger command "SUSPEND"
        error(traceback(thread, tostring(command)))
    end
    if command == "SUSPEND" then
        dispatch_wakeup()
        dispatch_error_queue()
    elseif command == "QUIT" then
        -- service exit
        return
    elseif command == "USER" then
        -- See skynet.coutine for detail
        error("Call skynet.coroutine.yield out of skynet.coroutine.resume\n" .. traceback(thread))
    elseif command == nil then
        -- debug trace
        return
    else
        error("Unknown command : " .. command .. "\n" .. traceback(thread))
    end
end

local co_create_trace_timeout
local timeout_traceback

---
--- trace timeout
--- @param on boolean enable|disable trace
function skynet.trace_timeout(on)
    local function co_create_trace(func, ti)
        local thread
        thread = co_create(function()
            timeout_traceback[thread] = nil
            func()
        end)
        local info = string.format("TIMER %d+%d : ", skynet.now(), ti)
        timeout_traceback[thread] = traceback(info, 3)
        return thread
    end
    if on then
        timeout_traceback = timeout_traceback or {}
        co_create_trace_timeout = co_create_trace
    else
        timeout_traceback = nil
        co_create_trace_timeout = co_create
    end
end

-- turn off by default
skynet.trace_timeout(false)

---
--- start a timer
--- @param func function timer callback
function skynet.timeout(ti, func)
    -- set timer
    local session_id = skynet_core.intcommand("TIMEOUT", ti)
    assert(session_id)

    -- start a thread, save session & thread relations (used to wait timer)
    local thread = co_create_trace_timeout(func, ti)
    assert(session_thread_map[session_id] == nil)
    session_thread_map[session_id] = thread

    return thread
end

--
local function suspend_sleep(session_id, token)
    local tag = thread_trace_tag_map[current_thread]
    if tag then
        skynet_core.trace(tag, "sleep", 2)
    end
    session_thread_map[session_id] = current_thread
    assert(sleep_session[token] == nil, "token duplicative")
    sleep_session[token] = session_id

    return thread_yield("SUSPEND")
end

-- ------------------------------------------------------
-- thread functions
-- ------------------------------------------------------

function skynet.sleep(ti, token)
    local session_id = skynet_core.intcommand("TIMEOUT", ti)
    assert(session_id)
    token = token or thread_running()
    local succ, ret = suspend_sleep(session_id, token)
    sleep_session[token] = nil
    if succ then
        return
    end
    if ret == "BREAK" then
        return "BREAK"
    else
        error(ret)
    end
end

function skynet.yield()
    return skynet.sleep(0)
end

function skynet.wait(token)
    local session_id = skynet_core.gen_session_id()
    token = token or thread_running()
    local ret, msg = suspend_sleep(session_id, token)
    sleep_session[token] = nil
    session_thread_map[session_id] = nil
end

function skynet.wakeup(token)
    if sleep_session[token] then
        table_insert(wakeup_queue, token)
        return true
    end
end

function skynet.fork(func, ...)
    local n = select("#", ...)
    local thread
    if n == 0 then
        thread = co_create(func)
    else
        local args = { ... }
        thread = co_create(function()
            func(table.unpack(args, 1, n))
        end)
    end
    table_insert(fork_queue, thread)
    return thread
end

-- ------------------------------------------------------
--
-- ------------------------------------------------------

-- self service handle
function skynet.self()
    return skynet_core.addresscommand("REG")
end

-- query local serivce name
function skynet.localname(name)
    return skynet_core.addresscommand("QUERY", name)
end

skynet.now = skynet_core.now    -- current tick (1tick = 10ms)
skynet.hpc = skynet_core.hpc    -- high performance counter

local trace_id = 0

---
--- trace thread info
--- @param info string trace thread info
function skynet.trace(info)
    skynet.log("TRACE", thread_trace_tag_map[current_thread])

    -- force off trace log
    if thread_trace_tag_map[current_thread] == false then
        return
    end

    trace_id = trace_id + 1
    local trace_tag = string.format(":%08x-%d", skynet.self(), trace_id)
    thread_trace_tag_map[current_thread] = trace_tag
    if info then
        skynet_core.trace(trace_tag, "trace " .. info)
    else
        skynet_core.trace(trace_tag, "trace")
    end
end

--- get trace current thread info
function skynet.tracetag()
    return thread_trace_tag_map[current_thread]
end

-- ------------------------------------------------------
-- time functions
-- ------------------------------------------------------

local starttime

--
function skynet.starttime()
    if not starttime then
        starttime = skynet_core.intcommand("START_TIME")
    end
    return starttime
end

-- get current time (seconds)
function skynet.time()
    return skynet.now() / 100 + (starttime or skynet.starttime())
end

-- ------------------------------------------------------
-- env functions
-- ------------------------------------------------------

function skynet.getenv(key)
    return (skynet_core.command("GETENV", key))
end

function skynet.setenv(key, value)
    assert(skynet_core.command("GETENV", key) == nil, "Can't setenv exist key : " .. key)
    skynet_core.command("SETENV", key .. " " .. value)
end


-- ------------------------------------------------------
--
-- ------------------------------------------------------


function skynet.send(addr, svc_msg_type, ...)
    local svc_msg_handler = svc_msg_handlers[svc_msg_type]
    return skynet_core.send(addr, svc_msg_handler.msg_type, 0, svc_msg_handler.pack(...))
end

function skynet.rawsend(addr, svc_msg_type, msg, msg_sz)
    local svc_msg_handler = svc_msg_handlers[svc_msg_type]
    return skynet_core.send(addr, svc_msg_handler.msg_type, 0, msg, msg_sz)
end

--- new session id
skynet.gen_session_id = assert(skynet_core.gen_session_id)

--- redirect the message to destination service
skynet.redirect = function(dst_svc_handle, src_svc_handle, svc_msg_type, ...)
    return skynet_core.redirect(dst_svc_handle, src_svc_handle, svc_msg_handlers[svc_msg_type].msg_type, ...)
end

skynet.pack = assert(skynet_core.pack)
skynet.pack_string = assert(skynet_core.pack_string)
skynet.unpack = assert(skynet_core.unpack)
skynet.tostring = assert(skynet_core.tostring)
skynet.trash = assert(skynet_core.trash)

local function yield_call(svc_handle, session_id)
    watching_session[session_id] = svc_handle
    session_thread_map[session_id] = current_thread
    local succ, msg, msg_sz = thread_yield "SUSPEND"
    watching_session[session_id] = nil
    if not succ then
        error "call failed"
    end
    return msg, msg_sz
end

---
--- send message to destination service (will suspend thread until the service response)
--- @param addr string|number service name or service handle
--- @param svc_msg_type string service message type, e.g. "lua", "debug", ...
function skynet.call(addr, svc_msg_type, ...)
    local tag = thread_trace_tag_map[current_thread]
    if tag then
        skynet_core.trace(tag, "call", 2)
        skynet_core.send(addr, skynet.SERVICE_MSG_TYPE_TRACE, 0, tag)
    end

    local svc_msg_handler = svc_msg_handlers[svc_msg_type]
    local session_id = skynet_core.send(addr, svc_msg_handler.msg_type, nil, svc_msg_handler.pack(...))
    if session_id == nil then
        error("call to invalid address " .. skynet.to_address(addr))
    end
    return svc_msg_handler.unpack(yield_call(addr, session_id))
end

---
--- send message to destination service (will suspend thread until the service response)
--- @param addr string|number service name or service handle
--- @param svc_msg_type string service message type, e.g. "lua", "debug", ...
--- @param msg string message
--- @param msg_sz number message size
function skynet.call_raw(addr, svc_msg_type, msg, msg_sz)
    local tag = thread_trace_tag_map[current_thread]
    if tag then
        skynet_core.trace(tag, "call", 2)
        skynet_core.send(addr, skynet.SERVICE_MSG_TYPE_TRACE, 0, tag)
    end
    local svc_msg_handler = svc_msg_handlers[svc_msg_type]
    local session_id = assert(skynet_core.send(addr, svc_msg_handler.msg_type, nil, msg, msg_sz), "call to invalid address")
    return yield_call(addr, session_id)
end

---
--- send message to destination service (will suspend thread until the service response)
--- @param tag string trace tag
--- @param addr string|number service name or service handle
--- @param svc_msg_type string service message type, e.g. "lua", "debug", ...
--- @param msg string message
--- @param msg_sz number message size
function skynet.call_trace(tag, addr, svc_msg_type, msg, msg_sz)
    skynet_core.trace(tag, "call trace begin")
    skynet_core.send(addr, skynet.SERVICE_MSG_TYPE_TRACE, 0, tag)
    local svc_msg_handler = svc_msg_handlers[svc_msg_type]
    local session_id = assert(skynet_core.send(addr, svc_msg_handler.msg_type, nil, msg, msg_sz), "call to invalid address")
    local msg, msg_sz = yield_call(addr, session_id)
    skynet_core.trace(tag, "call trace end")
    return msg, msg_sz
end

---
--- release message
--- @param msg string message
--- @param msg_sz number message size
--- @return
function skynet.ret(msg, msg_sz)
    msg = msg or ""
    local trace_tag = thread_trace_tag_map[current_thread]
    if trace_tag then
        skynet_core.trace(trace_tag, "response")
    end

    local co_session_id = thread_session_map[current_thread]
    thread_session_map[current_thread] = nil
    if co_session_id == 0 then
        if msg_sz ~= nil then
            skynet_core.trash(msg, msg_sz)
        end
        return false    -- send don't need ret
    end

    local co_svc_handle = thread_svc_handle_map[current_thread]
    if not co_session_id then
        error "No session"
    end

    --
    local ret = skynet_core.send(co_svc_handle, skynet.SERVICE_MSG_TYPE_RESPONSE, co_session_id, msg, msg_sz)
    if ret then
        return true
    elseif ret == false then
        -- If the package is too large, returns false. so we should report error back
        skynet_core.send(co_svc_handle, skynet.SERVICE_MSG_TYPE_ERROR, co_session_id, "")
    end

    return false
end

---
--- get current thread context
--- @return session_id, address
function skynet.context()
    local session_id = thread_session_map[current_thread]
    local svc_handle = thread_svc_handle_map[current_thread]
    return session_id, svc_handle
end

function skynet.ignoreret()
    -- We use session for other uses
    thread_session_map[current_thread] = nil
end

function skynet.response(pack)
    pack = pack or skynet.pack

    local co_session_id = assert(thread_session_map[current_thread], "no session")
    thread_session_map[current_thread] = nil
    local co_svc_handle = thread_svc_handle_map[current_thread]
    if co_session_id == 0 then
        --  do not response when session_id == 0 (send)
        return function() end
    end
    local function response(ok, ...)
        if ok == "TEST" then
            return unresponse[response] ~= nil
        end
        if not pack then
            error "Can't response more than once"
        end

        local ret
        if unresponse[response] then
            if ok then
                ret = skynet_core.send(co_svc_handle, skynet.SERVICE_MSG_TYPE_RESPONSE, co_session_id, pack(...))
                if ret == false then
                    -- If the package is too large, returns false. so we should report error back
                    skynet_core.send(co_svc_handle, skynet.SERVICE_MSG_TYPE_ERROR, co_session_id, "")
                end
            else
                ret = skynet_core.send(co_svc_handle, skynet.SERVICE_MSG_TYPE_ERROR, co_session_id, "")
            end
            unresponse[response] = nil
            ret = ret ~= nil
        else
            ret = false
        end
        pack = nil
        return ret
    end
    unresponse[response] = co_svc_handle

    return response
end

function skynet.retpack(...)
    return skynet.ret(skynet.pack(...))
end

function skynet.dispatch(svc_msg_type, func)
    local svc_msg_handler = svc_msg_handlers[svc_msg_type]
    if func then
        local ret = svc_msg_handler.dispatch
        svc_msg_handler.dispatch = func
        return ret
    else
        return svc_msg_handler and svc_msg_handler.dispatch
    end
end

local function unknown_request(session_id, address, msg, msg_sz, svc_msg_type)
    skynet.log(string.format("Unknown request (%s): %s", svc_msg_type, skynet_core.tostring(msg, msg_sz)))
    error(string.format("Unknown session : %d from %x", session_id, address))
end

function skynet.dispatch_unknown_request(unknown)
    local prev = unknown_request
    unknown_request = unknown
    return prev
end

local function unknown_response(session_id, address, msg, msg_sz)
    skynet.log(string.format("Response message : %s", skynet_core.tostring(msg, msg_sz)))
    error(string.format("Unknown session : %d from %x", session_id, address))
end

function skynet.dispatch_unknown_response(unknown)
    local prev = unknown_response
    unknown_response = unknown
    return prev
end


local trace_source = {}

local function raw_dispatch_message(svc_msg_type, msg, msg_sz, session_id, src_svc_handle)
    --
    if svc_msg_type == skynet.SERVICE_MSG_TYPE_RESPONSE then
        local thread = session_thread_map[session_id]
        if thread == "BREAK" then
            session_thread_map[session_id] = nil
        elseif thread == nil then
            unknown_response(session_id, src_svc_handle, msg, msg_sz)
        else
            local tag = thread_trace_tag_map[thread]
            if tag then
                skynet_core.trace(tag, "resume")
            end
            session_thread_map[session_id] = nil
            suspend(thread, thread_resume(thread, true, msg, msg_sz))
        end
    else
        local svc_msg_handler = svc_msg_handlers[svc_msg_type]
        if svc_msg_handler == nil then
            if svc_msg_type == skynet.SERVICE_MSG_TYPE_TRACE then
                -- trace next request
                trace_source[src_svc_handle] = skynet_core.tostring(msg, msg_sz)
            elseif session_id ~= 0 then
                skynet_core.send(src_svc_handle, skynet.SERVICE_MSG_TYPE_ERROR, session_id, "")
            else
                unknown_request(session_id, src_svc_handle, msg, msg_sz, svc_msg_type)
            end
            return
        end

        local func = svc_msg_handler.dispatch
        if func then
            local thread = co_create(func)
            thread_session_map[thread] = session_id
            thread_svc_handle_map[thread] = src_svc_handle
            local traceflag = svc_msg_handler.trace
            if traceflag == false then
                -- force off
                trace_source[src_svc_handle] = nil
                thread_trace_tag_map[thread] = false
            else
                local tag = trace_source[src_svc_handle]
                if tag then
                    trace_source[src_svc_handle] = nil
                    skynet_core.trace(tag, "request")
                    thread_trace_tag_map[thread] = tag
                elseif traceflag then
                    -- set current_thread for trace
                    current_thread = thread
                    skynet.trace()
                end
            end
            suspend(thread, thread_resume(thread, session_id, src_svc_handle, svc_msg_handler.unpack(msg, msg_sz)))
        else
            trace_source[src_svc_handle] = nil
            if session_id ~= 0 then
                skynet_core.send(src_svc_handle, skynet.SERVICE_MSG_TYPE_ERROR, session_id, "")
            else
                unknown_request(session_id, src_svc_handle, msg, msg_sz, svc_msg_handlers[svc_msg_type].name)
            end
        end
    end
end

function skynet.dispatch_message(...)
    local succ, err = pcall(raw_dispatch_message, ...)
    while true do
        local thread = table_remove(fork_queue, 1)
        if thread == nil then
            break
        end
        local fork_succ, fork_err = pcall(suspend, thread, thread_resume(thread))
        if not fork_succ then
            if succ then
                succ = false
                err = tostring(fork_err)
            else
                err = tostring(err) .. "\n" .. tostring(fork_err)
            end
        end
    end
    assert(succ, tostring(err))
end

function skynet.newservice(name, ...)
    return skynet.call(".launcher", "lua", "LAUNCH", "snlua", name, ...)
end

function skynet.uniqueservice(global, ...)
    if global == true then
        return assert(skynet.call(".service", "lua", "GLAUNCH", ...))
    else
        return assert(skynet.call(".service", "lua", "LAUNCH", global, ...))
    end
end

function skynet.queryservice(global, ...)
    if global == true then
        return assert(skynet.call(".service", "lua", "GQUERY", ...))
    else
        return assert(skynet.call(".service", "lua", "QUERY", global, ...))
    end
end

--- convert addr to string (format: ':00000001')
--- @param addr string|number
function skynet.to_address(addr)
    if type(addr) == "number" then
        return string.format(":%08x", addr)
    else
        return tostring(addr)
    end
end

skynet.log = skynet_core.log
skynet.tracelog = skynet_core.trace

-- true: force on
-- false: force off
-- nil: optional (use skynet.trace() to trace one message)
function skynet.traceproto(svc_msg_type, flag)
    local svc_msg_handler = assert(svc_msg_handlers[svc_msg_type])
    svc_msg_handler.trace = flag
end

-- ------------------------------------------------------
-- register protocol
-- ------------------------------------------------------

skynet.register_protocol({
    msg_type_name = "lua",
    msg_type = skynet.SERVICE_MSG_TYPE_LUA,
    pack = skynet.pack,
    unpack = skynet.unpack,
})

skynet.register_protocol({
    msg_type_name = "response",
    msg_type = skynet.SERVICE_MSG_TYPE_RESPONSE,
})

skynet.register_protocol({
    msg_type_name = "error",
    msg_type = skynet.SERVICE_MSG_TYPE_ERROR,
    unpack = function(...)
        return ...
    end,
    dispatch = _error_dispatch,
})

-- ------------------------------------------------------
-- skynet module two-step initialize.
-- When you require a skynet module:
-- 1. Run module main function as official lua module behavior.
-- 2. Run the functions register by skynet.init() during the step 1, unless calling `require` in main thread .
--
-- If you call `require` in main thread ( service main function ), the functions
-- registered by skynet.init() do not execute immediately, they will be executed
-- by skynet.start() before start function.
-- ------------------------------------------------------

local init_func = {}

--
function skynet.init(func, name)
    assert(type(func) == "function")
    if init_func == nil then
        func()
    else
        table_insert(init_func, func)
        if name then
            assert(type(name) == "string")
            assert(init_func[name] == nil)
            init_func[name] = func
        end
    end
end

--
local function init_all()
    local funcs = init_func
    init_func = nil
    if funcs then
        for _, func in ipairs(funcs) do
            func()
        end
    end
end

local function ret(func, ...)
    func()
    return ...
end

local function init_template(start_func, ...)
    init_all()
    init_func = {}
    return ret(init_all, start_func(...))
end

function skynet.pcall(start_func, ...)
    return xpcall(init_template, traceback, start_func, ...)
end

---
--- initialize service
--- @param start_func function service start function
function skynet.init_service(start_func)
    local ok, err = skynet.pcall(start_func)
    if not ok then
        skynet.log("init service failed: " .. tostring(err))
        skynet.send(".launcher", "lua", "ERROR")
        skynet.exit()
    else
        skynet.send(".launcher", "lua", "LAUNCHOK")
    end
end

---
--- start service
--- @param start_func function service start function
function skynet.start(start_func)
    skynet_core.callback(skynet.dispatch_message)
    init_thread = skynet.timeout(0, function()
        skynet.init_service(start_func)
        init_thread = nil
    end)
end

---
--- exit service
function skynet.exit()
    fork_queue = {}    -- no fork coroutine can be execute after skynet.exit
    skynet.send(".launcher", "lua", "REMOVE", skynet.self(), false)
    -- report the sources that call me
    for thread, session_id in pairs(thread_session_map) do
        local address = thread_svc_handle_map[thread]
        if session_id ~= 0 and address then
            skynet_core.send(address, skynet.SERVICE_MSG_TYPE_ERROR, session_id, "")
        end
    end
    for res in pairs(unresponse) do
        res(false)
    end
    -- report the sources I call but haven't return
    local tmp = {}
    for session_id, svc_handle in pairs(watching_session) do
        tmp[svc_handle] = true
    end
    for svc_handle in pairs(tmp) do
        skynet_core.send(svc_handle, skynet.SERVICE_MSG_TYPE_ERROR, 0, "")
    end
    skynet_core.command("EXIT")
    -- quit service
    thread_yield("QUIT")
end

-- query whether the service is blocked
function skynet.endless()
    return (skynet_core.intcommand("STAT", "endless") == 1)
end

-- query the service message queue length
function skynet.mqlen()
    return skynet_core.intcommand("STAT", "mqlen")
end

-- query the service status
function skynet.stat(what)
    return skynet_core.intcommand("STAT", what)
end

--
function skynet.task(ret)
    if ret == nil then
        local t = 0
        for session_id, thread in pairs(session_thread_map) do
            t = t + 1
        end
        return t
    end
    if ret == "init" then
        if init_thread then
            return traceback(init_thread)
        else
            return
        end
    end

    local tt = type(ret)
    if tt == "table" then
        for session_id, thread in pairs(session_thread_map) do
            if timeout_traceback and timeout_traceback[thread] then
                ret[session_id] = timeout_traceback[thread]
            else
                ret[session_id] = traceback(thread)
            end
        end
        return
    elseif tt == "number" then
        local thread = session_thread_map[ret]
        if thread then
            return traceback(thread)
        else
            return "No session"
        end
    elseif tt == "thread" then
        for session_id, thread in pairs(session_thread_map) do
            if thread == ret then
                return session_id
            end
        end
        return
    end
end

function skynet.uniqtask()
    local stacks = {}
    for session_id, thread in pairs(session_thread_map) do
        local stack = traceback(thread)
        local info = stacks[stack] or { count = 0, sessions = {} }
        info.count = info.count + 1
        if info.count < 10 then
            info.sessions[#info.sessions + 1] = session_id
        end
        stacks[stack] = info
    end
    local ret = {}
    for stack, info in pairs(stacks) do
        local count = info.count
        local sessions = table.concat(info.sessions, ",")
        if count > 10 then
            sessions = sessions .. "..."
        end
        local head_line = string.format("%d\tsessions:[%s]\n", count, sessions)
        ret[head_line] = stack
    end
    return ret
end

function skynet.term(svc_handle)
    return _error_dispatch(0, svc_handle)
end

function skynet.memlimit(bytes)
    debug.getregistry().memlimit = bytes
    skynet.memlimit = nil    -- set only once
end

-- Inject internal debug framework
local skynet_debug = require "skynet.debug"
skynet_debug.init(skynet, {
    dispatch = skynet.dispatch_message,
    suspend = suspend,
})

return skynet
