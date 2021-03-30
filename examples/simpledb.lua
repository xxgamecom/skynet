local skynet = require "skynet"
require "skynet.manager"

local db = {}

local CMD = {}

function CMD.GET(key)
    return db[key]
end

function CMD.SET(key, value)
    local last = db[key]
    db[key] = value
    return last
end

skynet.start(function()
    skynet.dispatch("lua", function(session, address, cmd, ...)
        cmd = cmd:upper()
        if cmd == "PING" then
            assert(session == 0)
            local str = (...)
            if #str > 20 then
                str = str:sub(1, 20) .. "...(" .. #str .. ")"
            end
            skynet.log_info(string.format("%s ping %s", skynet.to_address(address), str))
            return
        end
        local f = CMD[cmd]
        if f then
            skynet.ret(skynet.pack(f(...)))
        else
            error(string.format("Unknown command %s", tostring(cmd)))
        end
    end)
    --	skynet.traceproto("lua", false)	-- true off tracelog
    skynet.register("SIMPLEDB")
end)
