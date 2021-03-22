--
-- bootstrap service
--
-- config file param:
-- bootstrap = "snlua bootstrap"
--

local skynet = require "skynet"
local skynet_service = require "skynet.service"
require "skynet.manager" -- import skynet.launch, ...

skynet.start(function()
    -- launcher service
    local launcher = assert(skynet.launch("snlua", "launcher"))
    skynet.name(".launcher", launcher)

    -- datacenterd service
    local datacenter = skynet.newservice("datacenterd")
    skynet.name("DATACENTER", datacenter)

    -- service_mgr service
    skynet.newservice("service_mgr")

    -- enable ssl
    local enablessl = skynet.getenv("enablessl")
    if enablessl then
        skynet_service.new("ltls_holder", function()
            local c = require "ltls.init.c"
            c.constructor()
        end)
    end

    -- start user service: main script
    pcall(skynet.newservice, skynet.getenv("start") or "main")

    skynet.exit()
end)
