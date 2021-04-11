local skynet = require "skynet"
local sprotoloader = require "sprotoloader"

local max_client = 64

skynet.start(function()
    print("Server start")
    skynet.uniqueservice("protoloader")
    local console = skynet.newservice("console")
    skynet.newservice("debug_console", 6000)
    skynet.newservice("simpledb")
    local watchdog = skynet.newservice("wswatchdog")
    skynet.call(watchdog, "lua", "start", {
        port = 5888,
        max_client = max_client,
        nodelay = true,
    })
    print("Watchdog listen on ", 5888)

    skynet.exit()
end)
