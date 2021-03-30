local skynet = require("skynet")
local skynet_co = require("skynet.coroutine")

local profile = require("skynet.profile")

local function co_func()
    skynet.log_info("resume co_func 1")

    local t = skynet_co.yield("BEGIN")
    skynet.log_info("resume co_func 2", t)

    t = skynet_co.yield("END")
    skynet.log_info("resume co_func 3", t)

    local ti = profile.stop(skynet_co.thread())
    skynet.log_info("cost time:", ti)
end

skynet.start(function()
    skynet.log_info("main", skynet_co.thread())

    profile.start()

    local f = skynet_co.wrap(co_func)
    skynet.log_info("step 1", f())
    skynet.log_info("step 2", f())
    skynet.log_info("step 3", f())

    skynet.exit()
end)

