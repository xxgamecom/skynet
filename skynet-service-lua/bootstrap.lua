local skynet = require "skynet"
local skynet_service = require "skynet.service"
require "skynet.manager" -- import skynet.launch, ...

skynet.start(function()
    --
    local launcher = assert(skynet.launch("snlua", "launcher"))
    skynet.name(".launcher", launcher)

    -- no service slave, use cdummy instead
    -- TODO: delete cslave.lua & cdummy.lua
    local ok, slave = pcall(skynet.newservice, "cdummy")
    if not ok then
        skynet.abort()
    end
    skynet.name(".cslave", slave)

    -- new global service: DATACENTER
    local datacenter = skynet.newservice("datacenterd")
    skynet.name("DATACENTER", datacenter)

    -- new service service_mgr
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
