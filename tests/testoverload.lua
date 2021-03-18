local skynet = require "skynet"

local mode = ...

if mode == "slave" then

local CMD = {}

function CMD.sum(n)
	skynet.log("for loop begin")
	local s = 0
	for i = 1, n do
		s = s + i
	end
	skynet.log("for loop end")
end

function CMD.blackhole()
end

skynet.start(function()
	skynet.dispatch("lua", function(_,_, cmd, ...)
		local f = CMD[cmd]
		f(...)
	end)
end)

else

skynet.start(function()
	local slave = skynet.newservice(SERVICE_NAME, "slave")
	for step = 1, 20 do
		skynet.log("overload test ".. step)
		for i = 1, 512 * step do
			skynet.send(slave, "lua", "blackhole")
		end
		skynet.sleep(step)
	end
	local n = 1000000000
	skynet.log(string.format("endless test n=%d", n))
	skynet.send(slave, "lua", "sum", n)
end)

end
