root = "./"
thread = 8
logger = nil
address = "127.0.0.1:2527"
master = "127.0.0.1:2013"
start = "testmulticast2"	-- main script
bootstrap = "snlua bootstrap"	-- The service for bootstrap
--standalone = "0.0.0.0:2013"
luaservice = root.."service/?.lua;"..root.."test/?.lua;"..root.."examples/?.lua"
lualoader = "lualib/loader.lua"
-- preload = "./examples/preload.lua"	-- run preload.lua before every lua service run
snax = root.."examples/?.lua;"..root.."test/?.lua"
cservice_path = root.."cservice/?.so"
