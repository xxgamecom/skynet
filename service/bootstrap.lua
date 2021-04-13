local skynet = require "skynet"
require "skynet.manager"

skynet.start(function()
    -- start launcher service
    local launcher = assert(skynet.launch("snlua", "launcher"))
    skynet.name(".launcher", launcher)

    -- start service_mgr service
    skynet.newservice("service_mgr")
    pcall(skynet.newservice, skynet.get_env("start") or "main")

    --
    skynet.exit()
end)
