-- read https://github.com/cloudwu/skynet/wiki/FAQ for the module "skynet.core"
local skynet_core = require "skynet.core"

local tostring = tostring
local coroutine = coroutine
local assert = assert
local pairs = pairs
local pcall = pcall
local table = table
local table_remove = table.remove
local table_insert = table.insert
local traceback = debug.traceback

local profile = require "skynet.profile"

local cresume = profile.resume
local running_thread = nil
local init_thread = nil

local function coroutine_resume(co, ...)
    running_thread = co
    return cresume(co, ...)
end
local coroutine_yield = profile.yield
local coroutine_create = coroutine.create

local msg_proto_handlers = {}

local skynet = {
    MSG_PTYPE_TEXT = 0,
    MSG_PTYPE_RESPONSE = 1,
    MSG_PTYPE_MULTICAST = 2,
    MSG_PTYPE_CLIENT = 3,
    MSG_PTYPE_SYSTEM = 4,
    MSG_PTYPE_HARBOR = 5,
    MSG_PTYPE_SOCKET = 6,
    MSG_PTYPE_ERROR = 7,
    MSG_PTYPE_QUEUE = 8, -- used in deprecated mqueue, use skynet.queue instead
    MSG_PTYPE_DEBUG = 9,
    MSG_PTYPE_LUA = 10,
    MSG_PTYPE_SNAX = 11,
    MSG_PTYPE_TRACE = 12, -- use for debug trace
}

-- code cache
skynet.cache = require "skynet.codecache"

-- register message protocol handler
function skynet.register_protocol(proto_handler)
    local msg_ptype_name = proto_handler.msg_ptype_name
    local msg_ptype = proto_handler.msg_ptype
    assert(msg_proto_handlers[msg_ptype_name] == nil and msg_proto_handlers[msg_ptype] == nil)
    assert(type(msg_ptype_name) == "string" and type(msg_ptype) == "number" and msg_ptype >= 0 and msg_ptype <= 255)
    msg_proto_handlers[msg_ptype_name] = proto_handler
    msg_proto_handlers[msg_ptype] = proto_handler
end

local session_id_coroutine = {}         -- session_id -> coroutine
local session_coroutine_id = {}         -- coroutine -> session id
local session_coroutine_address = {}    -- coroutine -> service handle
local session_coroutine_tracetag = {}   -- coroutine -> trace tag
local unresponse = {}

local wakeup_queue = {}
local sleep_session = {}

local watching_session = {}
local fork_queue = {}

-- thread suspend function
local suspend


----- monitor exit

local error_queue = {}

local function dispatch_error_queue()
    local session_id = table_remove(error_queue, 1)
    if session_id then
        local co = session_id_coroutine[session_id]
        session_id_coroutine[session_id] = nil
        return suspend(co, coroutine_resume(co, false))
    end
end

local function _error_dispatch(error_session, error_source)
    skynet.ignoreret()    -- don't return for error
    if error_session == 0 then
        -- error_source is down, clear unreponse set
        for resp, address in pairs(unresponse) do
            if error_source == address then
                unresponse[resp] = nil
            end
        end
        for session_id, srv in pairs(watching_session) do
            if srv == error_source then
                table_insert(error_queue, session_id)
            end
        end
    else
        -- capture an error for error_session
        if watching_session[error_session] then
            table_insert(error_queue, error_session)
        end
    end
end

-- coroutine reuse

local coroutine_pool = setmetatable({}, { __mode = "kv" })

local function co_create(f)
    local co = table_remove(coroutine_pool)
    if co == nil then
        co = coroutine_create(function(...)
            f(...)
            while true do
                local session_id = session_coroutine_id[co]
                if session_id and session_id ~= 0 then
                    local source = debug.getinfo(f, "S")
                    skynet.log(string.format("Maybe forgot response session %s from %s : %s:%d",
                            session_id,
                            skynet.address(session_coroutine_address[co]),
                            source.source, source.linedefined))
                end
                -- coroutine exit
                local tag = session_coroutine_tracetag[co]
                if tag ~= nil then
                    if tag then
                        skynet_core.trace(tag, "end")
                    end
                    session_coroutine_tracetag[co] = nil
                end
                local address = session_coroutine_address[co]
                if address then
                    session_coroutine_id[co] = nil
                    session_coroutine_address[co] = nil
                end

                -- recycle co into pool
                f = nil
                coroutine_pool[#coroutine_pool + 1] = co
                -- recv new main function f
                f = coroutine_yield "SUSPEND"
                f(coroutine_yield())
            end
        end)
    else
        -- pass the main function f to coroutine, and restore running thread
        local running = running_thread
        coroutine_resume(co, f)
        running_thread = running
    end
    return co
end

local function dispatch_wakeup()
    local token = table_remove(wakeup_queue, 1)
    if token then
        local session_id = sleep_session[token]
        if session_id then
            local co = session_id_coroutine[session_id]
            local tag = session_coroutine_tracetag[co]
            if tag then
                skynet_core.trace(tag, "resume")
            end
            session_id_coroutine[session_id] = "BREAK"
            return suspend(co, coroutine_resume(co, false, "BREAK"))
        end
    end
end

-- suspend is local function
function suspend(co, result, command)
    if not result then
        local session_id = session_coroutine_id[co]
        if session_id then
            -- coroutine may fork by others (session_id is nil)
            local addr = session_coroutine_address[co]
            if session_id ~= 0 then
                -- only call response error
                local tag = session_coroutine_tracetag[co]
                if tag then
                    skynet_core.trace(tag, "error")
                end
                skynet_core.send(addr, skynet.MSG_PTYPE_ERROR, session_id, "")
            end
            session_coroutine_id[co] = nil
        end
        session_coroutine_address[co] = nil
        session_coroutine_tracetag[co] = nil
        skynet.fork(function()
        end)    -- trigger command "SUSPEND"
        error(traceback(co, tostring(command)))
    end
    if command == "SUSPEND" then
        dispatch_wakeup()
        dispatch_error_queue()
    elseif command == "QUIT" then
        -- service exit
        return
    elseif command == "USER" then
        -- See skynet.coutine for detail
        error("Call skynet.coroutine.yield out of skynet.coroutine.resume\n" .. traceback(co))
    elseif command == nil then
        -- debug trace
        return
    else
        error("Unknown command : " .. command .. "\n" .. traceback(co))
    end
end

local co_create_for_timeout
local timeout_traceback

function skynet.trace_timeout(on)
    local function trace_coroutine(func, ti)
        local co
        co = co_create(function()
            timeout_traceback[co] = nil
            func()
        end)
        local info = string.format("TIMER %d+%d : ", skynet.now(), ti)
        timeout_traceback[co] = traceback(info, 3)
        return co
    end
    if on then
        timeout_traceback = timeout_traceback or {}
        co_create_for_timeout = trace_coroutine
    else
        timeout_traceback = nil
        co_create_for_timeout = co_create
    end
end

-- turn off by default
skynet.trace_timeout(false)

function skynet.timeout(ti, func)
    local session_id = skynet_core.intcommand("TIMEOUT", ti)
    assert(session_id)
    local co = co_create_for_timeout(func, ti)
    assert(session_id_coroutine[session_id] == nil)
    session_id_coroutine[session_id] = co
    -- for debug
    return co
end

--
local function suspend_sleep(session_id, token)
    local tag = session_coroutine_tracetag[running_thread]
    if tag then
        skynet_core.trace(tag, "sleep", 2)
    end
    session_id_coroutine[session_id] = running_thread
    assert(sleep_session[token] == nil, "token duplicative")
    sleep_session[token] = session_id

    return coroutine_yield "SUSPEND"
end

-- -----------------------------------
-- thread functions
-- -----------------------------------

function skynet.sleep(ti, token)
    local session_id = skynet_core.intcommand("TIMEOUT", ti)
    assert(session_id)
    token = token or coroutine.running()
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
    token = token or coroutine.running()
    local ret, msg = suspend_sleep(session_id, token)
    sleep_session[token] = nil
    session_id_coroutine[session_id] = nil
end

function skynet.wakeup(token)
    if sleep_session[token] then
        table_insert(wakeup_queue, token)
        return true
    end
end

function skynet.fork(func, ...)
    local n = select("#", ...)
    local co
    if n == 0 then
        co = co_create(func)
    else
        local args = { ... }
        co = co_create(function()
            func(table.unpack(args, 1, n))
        end)
    end
    table_insert(fork_queue, co)
    return co
end

-- -----------------------------------
--
-- -----------------------------------

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

local traceid = 0
function skynet.trace(info)
    skynet.log("TRACE", session_coroutine_tracetag[running_thread])
    if session_coroutine_tracetag[running_thread] == false then
        -- force off trace log
        return
    end
    traceid = traceid + 1

    local tag = string.format(":%08x-%d", skynet.self(), traceid)
    session_coroutine_tracetag[running_thread] = tag
    if info then
        skynet_core.trace(tag, "trace " .. info)
    else
        skynet_core.trace(tag, "trace")
    end
end

function skynet.tracetag()
    return session_coroutine_tracetag[running_thread]
end

-- -----------------------------------
-- time functions
-- -----------------------------------

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

-- -----------------------------------
-- env functions
-- -----------------------------------

function skynet.getenv(key)
    return (skynet_core.command("GETENV", key))
end

function skynet.setenv(key, value)
    assert(skynet_core.command("GETENV", key) == nil, "Can't setenv exist key : " .. key)
    skynet_core.command("SETENV", key .. " " .. value)
end


-- -----------------------------------
--
-- -----------------------------------


function skynet.send(addr, msg_ptype, ...)
    local proto_handler = msg_proto_handlers[msg_ptype]
    return skynet_core.send(addr, proto_handler.msg_ptype, 0, proto_handler.pack(...))
end

function skynet.rawsend(addr, msg_ptype, msg, sz)
    local proto_handler = msg_proto_handlers[msg_ptype]
    return skynet_core.send(addr, proto_handler.msg_ptype, 0, msg, sz)
end

--- new session id
skynet.gen_session_id = assert(skynet_core.gen_session_id)

--- redirect the message to destination service
skynet.redirect = function(dst_service_handle, src_svc_handle, msg_ptype, ...)
    return skynet_core.redirect(dst_service_handle, src_svc_handle, msg_proto_handlers[msg_ptype].msg_ptype, ...)
end

skynet.pack = assert(skynet_core.pack)
skynet.pack_string = assert(skynet_core.pack_string)
skynet.unpack = assert(skynet_core.unpack)
skynet.tostring = assert(skynet_core.tostring)
skynet.trash = assert(skynet_core.trash)

local function yield_call(service, session_id)
    watching_session[session_id] = service
    session_id_coroutine[session_id] = running_thread
    local succ, msg, sz = coroutine_yield "SUSPEND"
    watching_session[session_id] = nil
    if not succ then
        error "call failed"
    end
    return msg, sz
end

function skynet.call(addr, msg_ptype, ...)
    local tag = session_coroutine_tracetag[running_thread]
    if tag then
        skynet_core.trace(tag, "call", 2)
        skynet_core.send(addr, skynet.MSG_PTYPE_TRACE, 0, tag)
    end

    local proto_handler = msg_proto_handlers[msg_ptype]
    local session_id = skynet_core.send(addr, proto_handler.msg_ptype, nil, proto_handler.pack(...))
    if session_id == nil then
        error("call to invalid address " .. skynet.address(addr))
    end
    return proto_handler.unpack(yield_call(addr, session_id))
end

function skynet.rawcall(addr, msg_ptype, msg, sz)
    local tag = session_coroutine_tracetag[running_thread]
    if tag then
        skynet_core.trace(tag, "call", 2)
        skynet_core.send(addr, skynet.MSG_PTYPE_TRACE, 0, tag)
    end
    local proto_handler = msg_proto_handlers[msg_ptype]
    local session_id = assert(skynet_core.send(addr, proto_handler.msg_ptype, nil, msg, sz), "call to invalid address")
    return yield_call(addr, session_id)
end

function skynet.tracecall(tag, addr, msg_ptype, msg, sz)
    skynet_core.trace(tag, "tracecall begin")
    skynet_core.send(addr, skynet.MSG_PTYPE_TRACE, 0, tag)
    local proto_handler = msg_proto_handlers[msg_ptype]
    local session_id = assert(skynet_core.send(addr, proto_handler.msg_ptype, nil, msg, sz), "call to invalid address")
    local msg, sz = yield_call(addr, session_id)
    skynet_core.trace(tag, "tracecall end")
    return msg, sz
end

function skynet.ret(msg, sz)
    msg = msg or ""
    local tag = session_coroutine_tracetag[running_thread]
    if tag then
        skynet_core.trace(tag, "response")
    end
    local co_session = session_coroutine_id[running_thread]
    session_coroutine_id[running_thread] = nil
    if co_session == 0 then
        if sz ~= nil then
            skynet_core.trash(msg, sz)
        end
        return false    -- send don't need ret
    end
    local co_address = session_coroutine_address[running_thread]
    if not co_session then
        error "No session"
    end
    local ret = skynet_core.send(co_address, skynet.MSG_PTYPE_RESPONSE, co_session, msg, sz)
    if ret then
        return true
    elseif ret == false then
        -- If the package is too large, returns false. so we should report error back
        skynet_core.send(co_address, skynet.MSG_PTYPE_ERROR, co_session, "")
    end
    return false
end

function skynet.context()
    local co_session = session_coroutine_id[running_thread]
    local co_address = session_coroutine_address[running_thread]
    return co_session, co_address
end

function skynet.ignoreret()
    -- We use session for other uses
    session_coroutine_id[running_thread] = nil
end

function skynet.response(pack)
    pack = pack or skynet.pack

    local co_session = assert(session_coroutine_id[running_thread], "no session")
    session_coroutine_id[running_thread] = nil
    local co_address = session_coroutine_address[running_thread]
    if co_session == 0 then
        --  do not response when session_id == 0 (send)
        return function()
        end
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
                ret = skynet_core.send(co_address, skynet.MSG_PTYPE_RESPONSE, co_session, pack(...))
                if ret == false then
                    -- If the package is too large, returns false. so we should report error back
                    skynet_core.send(co_address, skynet.MSG_PTYPE_ERROR, co_session, "")
                end
            else
                ret = skynet_core.send(co_address, skynet.MSG_PTYPE_ERROR, co_session, "")
            end
            unresponse[response] = nil
            ret = ret ~= nil
        else
            ret = false
        end
        pack = nil
        return ret
    end
    unresponse[response] = co_address

    return response
end

function skynet.retpack(...)
    return skynet.ret(skynet.pack(...))
end

function skynet.dispatch(msg_ptype, func)
    local proto_handler = msg_proto_handlers[msg_ptype]
    if func then
        local ret = proto_handler.dispatch
        proto_handler.dispatch = func
        return ret
    else
        return proto_handler and proto_handler.dispatch
    end
end

local function unknown_request(session_id, address, msg, sz, msg_ptype)
    skynet.log(string.format("Unknown request (%s): %s", msg_ptype, skynet_core.tostring(msg, sz)))
    error(string.format("Unknown session : %d from %x", session_id, address))
end

function skynet.dispatch_unknown_request(unknown)
    local prev = unknown_request
    unknown_request = unknown
    return prev
end

local function unknown_response(session_id, address, msg, sz)
    skynet.log(string.format("Response message : %s", skynet_core.tostring(msg, sz)))
    error(string.format("Unknown session : %d from %x", session_id, address))
end

function skynet.dispatch_unknown_response(unknown)
    local prev = unknown_response
    unknown_response = unknown
    return prev
end


local trace_source = {}

local function raw_dispatch_message(msg_ptype, msg, sz, session_id, src_svc_handle)
    --
    if msg_ptype == skynet.MSG_PTYPE_RESPONSE then
        local co = session_id_coroutine[session_id]
        if co == "BREAK" then
            session_id_coroutine[session_id] = nil
        elseif co == nil then
            unknown_response(session_id, src_svc_handle, msg, sz)
        else
            local tag = session_coroutine_tracetag[co]
            if tag then
                skynet_core.trace(tag, "resume")
            end
            session_id_coroutine[session_id] = nil
            suspend(co, coroutine_resume(co, true, msg, sz))
        end
    else
        local proto_handler = msg_proto_handlers[msg_ptype]
        if proto_handler == nil then
            if msg_ptype == skynet.MSG_PTYPE_TRACE then
                -- trace next request
                trace_source[src_svc_handle] = skynet_core.tostring(msg, sz)
            elseif session_id ~= 0 then
                skynet_core.send(src_svc_handle, skynet.MSG_PTYPE_ERROR, session_id, "")
            else
                unknown_request(session_id, src_svc_handle, msg, sz, msg_ptype)
            end
            return
        end

        local f = proto_handler.dispatch
        if f then
            local co = co_create(f)
            session_coroutine_id[co] = session_id
            session_coroutine_address[co] = src_svc_handle
            local traceflag = proto_handler.trace
            if traceflag == false then
                -- force off
                trace_source[src_svc_handle] = nil
                session_coroutine_tracetag[co] = false
            else
                local tag = trace_source[src_svc_handle]
                if tag then
                    trace_source[src_svc_handle] = nil
                    skynet_core.trace(tag, "request")
                    session_coroutine_tracetag[co] = tag
                elseif traceflag then
                    -- set running_thread for trace
                    running_thread = co
                    skynet.trace()
                end
            end
            suspend(co, coroutine_resume(co, session_id, src_svc_handle, proto_handler.unpack(msg, sz)))
        else
            trace_source[src_svc_handle] = nil
            if session_id ~= 0 then
                skynet_core.send(src_svc_handle, skynet.MSG_PTYPE_ERROR, session_id, "")
            else
                unknown_request(session_id, src_svc_handle, msg, sz, msg_proto_handlers[msg_ptype].name)
            end
        end
    end
end

function skynet.dispatch_message(...)
    local succ, err = pcall(raw_dispatch_message, ...)
    while true do
        local co = table_remove(fork_queue, 1)
        if co == nil then
            break
        end
        local fork_succ, fork_err = pcall(suspend, co, coroutine_resume(co))
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

function skynet.address(addr)
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
function skynet.traceproto(msg_ptype, flag)
    local proto_handler = assert(msg_proto_handlers[msg_ptype])
    proto_handler.trace = flag
end

-- -----------------------------------
-- register protocol
-- -----------------------------------

skynet.register_protocol({
    msg_ptype_name = "lua",
    msg_ptype = skynet.MSG_PTYPE_LUA,
    pack = skynet.pack,
    unpack = skynet.unpack,
})

skynet.register_protocol({
    msg_ptype_name = "response",
    msg_ptype = skynet.MSG_PTYPE_RESPONSE,
})

skynet.register_protocol({
    msg_ptype_name = "error",
    msg_ptype = skynet.MSG_PTYPE_ERROR,
    unpack = function(...)
        return ...
    end,
    dispatch = _error_dispatch,
})

-- -----------------------------------
--
-- -----------------------------------

local init_func = {}

--
function skynet.init(f, name)
    assert(type(f) == "function")
    if init_func == nil then
        f()
    else
        table_insert(init_func, f)
        if name then
            assert(type(name) == "string")
            assert(init_func[name] == nil)
            init_func[name] = f
        end
    end
end

local function init_all()
    local funcs = init_func
    init_func = nil
    if funcs then
        for _, f in ipairs(funcs) do
            f()
        end
    end
end

local function ret(f, ...)
    f()
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

-- start service
function skynet.start(start_func)
    skynet_core.callback(skynet.dispatch_message)
    init_thread = skynet.timeout(0, function()
        skynet.init_service(start_func)
        init_thread = nil
    end)
end

-- exit service
function skynet.exit()
    fork_queue = {}    -- no fork coroutine can be execute after skynet.exit
    skynet.send(".launcher", "lua", "REMOVE", skynet.self(), false)
    -- report the sources that call me
    for co, session_id in pairs(session_coroutine_id) do
        local address = session_coroutine_address[co]
        if session_id ~= 0 and address then
            skynet_core.send(address, skynet.MSG_PTYPE_ERROR, session_id, "")
        end
    end
    for resp in pairs(unresponse) do
        resp(false)
    end
    -- report the sources I call but haven't return
    local tmp = {}
    for session_id, address in pairs(watching_session) do
        tmp[address] = true
    end
    for address in pairs(tmp) do
        skynet_core.send(address, skynet.MSG_PTYPE_ERROR, 0, "")
    end
    skynet_core.command("EXIT")
    -- quit service
    coroutine_yield "QUIT"
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
        for session_id, co in pairs(session_id_coroutine) do
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
        for session_id, co in pairs(session_id_coroutine) do
            if timeout_traceback and timeout_traceback[co] then
                ret[session_id] = timeout_traceback[co]
            else
                ret[session_id] = traceback(co)
            end
        end
        return
    elseif tt == "number" then
        local co = session_id_coroutine[ret]
        if co then
            return traceback(co)
        else
            return "No session"
        end
    elseif tt == "thread" then
        for session_id, co in pairs(session_id_coroutine) do
            if co == ret then
                return session_id
            end
        end
        return
    end
end

function skynet.uniqtask()
    local stacks = {}
    for session_id, co in pairs(session_id_coroutine) do
        local stack = traceback(co)
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

function skynet.term(service)
    return _error_dispatch(0, service)
end

function skynet.memlimit(bytes)
    debug.getregistry().memlimit = bytes
    skynet.memlimit = nil    -- set only once
end

-- Inject internal debug framework
local debug = require "skynet.debug"
debug.init(skynet, {
    dispatch = skynet.dispatch_message,
    suspend = suspend,
})

return skynet
