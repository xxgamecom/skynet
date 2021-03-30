local skynet = require "skynet"

local function term()
    skynet.log_info("Sleep one second, and term the call to UNEXIST")
    skynet.sleep(100)
    local self = skynet.self()
    skynet.send(skynet.self(), "debug", "TERM", "UNEXIST")
end

skynet.start(function()
    skynet.fork(term)
    skynet.log_info("call an unexist named service UNEXIST, may block")
    pcall(skynet.call, "UNEXIST", "lua", "test")
    skynet.log_info("unblock the unexisted service call")
end)
