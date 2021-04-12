local skynet = require "skynet"
local service = require "skynet.service"

local function test_service()
    local skynet = require "skynet"

    skynet.start(function()
        skynet.dispatch("lua", function()
            skynet.log_info("Wait for 1s")
            skynet.sleep(100)    -- response after 1s for any request
            skynet.ret()
        end)
    end)
end

local function timeout_call(ti, ...)
    local thread = {}
    local ret

    skynet.fork(function(...)
        ret = table.pack(pcall(skynet.call, ...))
        skynet.wakeup(thread)
    end, ...)

    skynet.sleep(ti, thread)
    if ret then
        if ret[1] then
            return table.unpack(ret, 1, ret.n)
        else
            error(ret[2])
        end
    else
        -- timeout
        return false
    end
end

skynet.start(function()
    local test = service.new("testtimeout", test_service)
    skynet.log_info("1", skynet.now())
    skynet.call(test, "lua")
    skynet.log_info("2", skynet.now())
    skynet.log_info(timeout_call(50, test, "lua"))
    skynet.log_info("3", skynet.now())
    skynet.log_info(timeout_call(150, test, "lua"))
    skynet.log_info("4", skynet.now())
    skynet.exit()
end)




