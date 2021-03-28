--
-- logger service helper
--

local skynet = require "skynet"

local M = {}

---
--- log error
function M.error(...)
    if skynet.getenv("log_level_error") == "false" then
        return
    end

    skynet.send(".loggerd", "lua", "error", "service_name:" .. SERVICE_NAME .. " svc_handle:" .. skynet.self(), ...)
end

---
--- log info
function M.info(...)
    if skynet.getenv("log_level_info") == "false" then
        return
    end

    skynet.send(".loggerd", "lua", "info", "service_name:" .. SERVICE_NAME .. " svc_handle:" .. skynet.self(), ...)
end

---
--- log warn
function M.warn(...)
    if skynet.getenv("log_level_warn") == "false" then
        return
    end

    skynet.send(".loggerd", "lua", "warning", "service_name:" .. SERVICE_NAME .. " svc_handle:" .. skynet.self(), ...)
end

---
--- log object data
---@param obj_name
---@param obj_id
function M.obj(obj_name, obj_id, ...)
    if not skynet.getenv("log_level_obj") then
        return
    end
    if obj_name == nil then
        obj_name = "object"
    end
    if obj_id == nil then
        obj_id = "objectid"
    end
    skynet.send(".loggerd", "lua", "obj", obj_name, obj_id, "service_name:" .. SERVICE_NAME .. " svc_handle:" .. skynet.self(), ...)
end

---
--- log net message
---@param msg_name
function M.net(msg_name, ...)
    if skynet.getenv("log_level_net") == "false" then
        return
    end

    if msg_name == nil or type(msg_name) ~= "string" then
        return
    end
    skynet.send(".loggerd", "lua", "net", msg_name, "service_name:" .. SERVICE_NAME .. " svc_handle:" .. skynet.self(), ...)
end

---
--- reload logger service
function M.reload()
    skynet.send(".loggerd", "lua", "reload")
end

---
--- exit logger service
function M.exit()
    skynet.send(".loggerd", "lua", "exit")
end

return M
