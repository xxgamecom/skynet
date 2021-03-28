--
-- message handler
--
-- examples:
-- local msg_handler = require("skynet.msg_handler")
--
-- -- register "lua" service message dispatch function
-- skynet.dispatch("lua", function(session, source, ...)
--     msg_handler:process(session, source, "lua", ...)
-- end)
--
-- -- add message process script
-- msg_handler:add_handler("cmd", "agent_svc_cmd")           -- cmd message process script agent_svc_cmd.lua
-- msg_handler:add_handler("request", "agent_svc_request")   -- request message process script agent_svc_request.lua
-- msg_handler:add_handler("notice", "agent_svc_notice")     -- notice message process script agent_svc_notice.lua
--
-- -- add message process script
-- msg_handler:add_handler("cmd", "auth_svc_cmd")
-- msg_handler:add_handler("request", "auth_svc_request")
-- msg_handler:add_handler("auth", "auth_svc_auth")
--

local skynet = require "skynet"
--local logger = require "LoggerServiceHelper"

--local SkynetUtils = require "utils.SkynetUtils"

local msg_handler = class("msg_handler")

function msg_handler:ctor()
    self.msg_handlers = {}
end

---
--- add message handler
--- @param msg_name string message name, e.g. cmd, dao ..., user custome message name
--- @param msg_handler string message handler script, use require to load
function msg_handler:add_handler(msg_name, msg_handler)
    self.msg_handlers[msg_name] = require(msg_handler)
end

---
--- process message
--- @param session_id number session id
--- @param src_svc_handle string source service handle
--- @param svc_msg_type string service message type, e.g. client, lua ...
--- @param msg_name string message name, e.g. cmd, dao ...
function msg_handler:process(session_id, src_svc_handle, svc_msg_type, msg_name, ...)
    -- check message name
    if msg_name == nil then
    --    logger.error("MsgHandler.process, msg name is nil")
        return
    end

    --logger.net(skynet.self() .. "__" .. svc_msg_type .. "_" .. msg_name .. "_request", ...)

    -- check message handler
    local msg_handler = self.msg_handlers[msg_name]
    if msg_handler == nil then
    --    logger.error("MsgHandler.process, no msg handler, msg_name: " .. msg_name, ...)
        return
    end

    -- process message
    --local status, info = SkynetUtils.pcall(msg_handler:process, session_id, src_svc_handle, ...)
    --if not status then
    --    logger.error("MsgHandler.process, call msg handler failed: ", info, ...)
    --end
end

return M
