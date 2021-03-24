local skynet = require "skynet"
require "skynet.manager" -- import skynet.launch, ...

skynet.start(function()
    --
    local launcher = assert(skynet.launch("snlua", "launcher"))
    skynet.name(".launcher", launcher)

    --
    local datacenter = skynet.newservice("datacenterd")
    skynet.name("DATACENTER", datacenter)

    --
    skynet.newservice "service_mgr"
    pcall(skynet.newservice, skynet.getenv("start") or "main")

    --
    skynet.exit()
end)
