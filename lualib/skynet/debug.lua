local table = table
local extern_dbgcmd = {}

local function init(skynet, export)
    local internal_info_func

    function skynet.info_func(func)
        internal_info_func = func
    end

    local dbg_cmd

    local function init_dbgcmd()
        dbg_cmd = {}

        function dbg_cmd.MEM()
            local kb = collectgarbage "count"
            skynet.ret(skynet.pack(kb))
        end

        function dbg_cmd.GC()

            collectgarbage "collect"
        end

        function dbg_cmd.STAT()
            local stat = {}
            stat.task = skynet.task()
            stat.mqlen = skynet.stat "mqlen"
            stat.cpu = skynet.stat "cpu"
            stat.message = skynet.stat "message"
            skynet.ret(skynet.pack(stat))
        end

        function dbg_cmd.TASK(session)
            if session then
                skynet.ret(skynet.pack(skynet.task(session)))
            else
                local task = {}
                skynet.task(task)
                skynet.ret(skynet.pack(task))
            end
        end

        function dbg_cmd.UNIQTASK()
            skynet.ret(skynet.pack(skynet.uniqtask()))
        end

        function dbg_cmd.INFO(...)
            if internal_info_func then
                skynet.ret(skynet.pack(internal_info_func(...)))
            else
                skynet.ret(skynet.pack(nil))
            end
        end

        function dbg_cmd.EXIT()
            skynet.exit()
        end

        function dbg_cmd.RUN(source, filename, ...)
            local inject = require "skynet.inject"
            local args = table.pack(...)
            local ok, output = inject(skynet, source, filename, args, export.dispatch, skynet.register_svc_msg_handler)
            collectgarbage "collect"
            skynet.ret(skynet.pack(ok, table.concat(output, "\n")))
        end

        function dbg_cmd.TERM(svc_handle)
            skynet.term(svc_handle)
        end

        function dbg_cmd.REMOTEDEBUG(...)
            local remotedebug = require "skynet.remotedebug"
            remotedebug.start(export, ...)
        end

        function dbg_cmd.SUPPORT(pname)
            return skynet.ret(skynet.pack(skynet.dispatch(pname) ~= nil))
        end

        function dbg_cmd.PING()
            return skynet.ret()
        end

        function dbg_cmd.LINK()
            skynet.response()    -- get response , but not return. raise error when exit
        end

        function dbg_cmd.TRACELOG(proto, flag)
            if type(proto) ~= "string" then
                flag = proto
                proto = "lua"
            end
            skynet.log(string.format("Turn trace log %s for %s", flag, proto))
            skynet.traceproto(proto, flag)
            skynet.ret()
        end

        return dbg_cmd
    end -- function init_dbgcmd

    local function _debug_dispatch(session, address, cmd, ...)
        dbg_cmd = dbg_cmd or init_dbgcmd() -- lazy init dbg_cmd
        local f = dbg_cmd[cmd] or extern_dbgcmd[cmd]
        assert(f, cmd)
        f(...)
    end

    skynet.register_svc_msg_handler({
        msg_type_name = "debug",
        msg_type = assert(skynet.SERVICE_MSG_TYPE_DEBUG),
        pack = assert(skynet.pack),
        unpack = assert(skynet.unpack),
        dispatch = _debug_dispatch,
    })
end

local function reg_debug_cmd(name, fn)
    extern_dbgcmd[name] = fn
end

return {
    init = init,
    reg_debug_cmd = reg_debug_cmd,
}
