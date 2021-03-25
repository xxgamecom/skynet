local skynet = require "skynet"
-- You should use skynet.coroutine instead of origin coroutine in skynet
local skynet_co = require "skynet.coroutine"
local profile = require "skynet.profile"

local function status(co)
    repeat
        local status = skynet_co.status(co)
        print("STATUS", status)
        skynet.sleep(100)
    until status == "suspended"

    repeat
        local ok, n = assert(skynet_co.resume(co))
        print("status thread", n)
    until not n
    skynet.exit()
end

local function test(n)
    local co = skynet_co.running()
    print("begin", co, skynet_co.thread(co))    -- false
    skynet.fork(status, co)
    for i = 1, n do
        skynet.sleep(100)
        skynet_co.yield(i)
    end
    print("end", co)
end

local function main()
    local f = skynet_co.wrap(test)
    skynet_co.yield("begin")
    for i = 1, 3 do
        local n = f(5)
        print("main thread", n)
    end
    skynet_co.yield("end")
    print("main thread time:", profile.stop(skynet_co.thread()))
end

skynet.start(function()
    print("Main thead :", skynet_co.thread())    -- true
    print(skynet_co.resume(skynet_co.running()))    -- always return false

    profile.start()

    local f = skynet_co.wrap(main)
    print("main step", f())
    print("main step", f())
    print("main step", f())
    --	print("main thread time:", profile.stop())
end)
