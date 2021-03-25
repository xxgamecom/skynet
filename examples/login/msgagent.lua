local skynet = require "skynet"

skynet.register_protocol {
    msg_ptype_name = "client",
    msg_ptype = skynet.SERVICE_MSG_TYPE_CLIENT,
    unpack = skynet.tostring,
}

local gate
local userid, subid

local CMD = {}

function CMD.login(source, uid, sid, secret)
    -- you may use secret to make a encrypted data stream
    skynet.log(string.format("%s is login", uid))
    gate = source
    userid = uid
    subid = sid
    -- you may load user data from database
end

local function logout()
    if gate then
        skynet.call(gate, "lua", "logout", userid, subid)
    end
    skynet.exit()
end

function CMD.logout(source)
    -- NOTICE: The logout MAY be reentry
    skynet.log(string.format("%s is logout", userid))
    logout()
end

function CMD.afk(source)
    -- the connection is broken, but the user may back
    skynet.log(string.format("AFK"))
end

skynet.start(function()
    -- If you want to fork a work thread , you MUST do it in CMD.login
    skynet.dispatch("lua", function(session, source, command, ...)
        local f = assert(CMD[command])
        skynet.ret(skynet.pack(f(source, ...)))
    end)

    skynet.dispatch("client", function(_, _, msg)
        -- the simple echo service
        skynet.sleep(10)    -- sleep a while
        skynet.ret(msg)
    end)
end)
