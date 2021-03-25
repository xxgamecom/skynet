local skynet = require "skynet"

local service_name = (...)

local CMD = {}

function CMD.init(code, ...)
    local start_func
    skynet.start = function(f)
        start_func = f
    end
    skynet.dispatch("lua", function()
        error("No dispatch function")
    end)
    local mainfunc = assert(load(code, service_name))
    assert(skynet.pcall(mainfunc, ...))
    if start_func then
        start_func()
    end
    skynet.ret()
end

skynet.start(function()
    skynet.dispatch("lua", function(_, _, cmd, ...)
        CMD[cmd](...)
    end)
end)
