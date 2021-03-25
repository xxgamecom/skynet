local skynet = require "skynet"
require "skynet.manager"

-- register protocol text before skynet.start would be better.
skynet.register_protocol {
    msg_ptype_name = "text",
    msg_ptype = skynet.SERVICE_MSG_TYPE_TEXT,
    unpack = skynet.tostring,
    dispatch = function(_, address, msg)
        print(string.format(":%08x(%.2f): %s", address, skynet.time(), msg))
    end
}

skynet.register_protocol({
    name = "SYSTEM",
    msg_ptype = skynet.SERVICE_MSG_TYPE_SYSTEM,
    unpack = function(...)
        return ...
    end,
    dispatch = function()
        -- reopen signal
        print("SIGHUP")
    end
})

skynet.start(function()
end)