local skynet = require("skynet")
local skynet_co = require("skynet.coroutine")

local profile = require("skynet.profile")

local function co_func()
    skynet.log("resume co_func 1")

    local t = skynet_co.yield("BEGIN")
    skynet.log("resume co_func 2", t)

    t = skynet_co.yield("END")
    skynet.log("resume co_func 3", t)

    local ti = profile.stop(skynet_co.thread())
    skynet.log("cost time:", ti)
end

skynet.start(function()
    skynet.log("main", skynet_co.thread())

    profile.start()

    local f = skynet_co.wrap(co_func)
    skynet.log("step 1", f())
    skynet.log("step 2", f())
    skynet.log("step 3", f())

    skynet.exit()
end)

