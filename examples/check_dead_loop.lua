local skynet = require "skynet"

local list = {}

local function timeout_check(ticks)
    if not next(list) then
        return
    end

    --
    skynet.sleep(ticks)

    --
    for k, v in pairs(list) do
        skynet.log_info("timout", ticks, k, v)
    end
end

skynet.start(function()
    skynet.log_info("ping all")
    local list_ret = skynet.call(".launcher", "lua", "LIST")
    for addr, desc in pairs(list_ret) do
        list[addr] = desc
        skynet.fork(function()
            skynet.call(addr, "debug", "INFO")
            list[addr] = nil
        end)
    end
    skynet.sleep(0)
    timeout_check(100)
    timeout_check(400)
    timeout_check(500)
    skynet.exit()
end)
