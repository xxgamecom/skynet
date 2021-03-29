-- ------------------------
-- path
-- ------------------------

luaservice = "./service/?.lua;./examples/login/?.lua"
cpath = "./cservice/?.so"

-- ------------------------
-- skynet node config
-- ------------------------

thread = 8
bootstrap = "snlua bootstrap"   -- The service for bootstrap
lualoader = "lualib/loader.lua"

-- main script
start = "main"

-- ------------------------
-- logger
-- ------------------------

