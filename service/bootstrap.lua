local skynet = require "skynet"
require "skynet.manager" -- import skynet.launch, ...

skynet.start(function()
    --
    local launcher = assert(skynet.launch("snlua", "launcher"))
    skynet.name(".launcher", launcher)

    --
    skynet.newservice "service_mgr"
    pcall(skynet.newservice, skynet.getenv("start") or "main")

    --
    skynet.exit()
end)
