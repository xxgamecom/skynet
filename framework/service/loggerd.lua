--
-- logger service
--

local skynet = require "skynet"
require "skynet.manager"

local file_helper = require "utils.file_helper"
local time_helper = require "utils.time_helper"
local table_helper = require "utils.table_helper"

local io = io
local os = os
local type = type
local pairs = pairs
local tostring = tostring

local file_info_map = {}    -- key: file path, value: file info ({ fd, has_data, write_time })

---
--- get log filename
---@param dirname string
---@param filename string
---@return string the full path of log
local function get_filename(dirname, filename)
    -- store divide by date
    local path = skynet.getenv("log_path")
    path = path or "."
    dirname = dirname or "."

    -- create log directory
    local current_time = os.date("%Y_%m_%d", time_helper.get_time())
    local log_path = path .. "/" .. current_time .. "/" .. dirname
    if not file_helper.is_exists(log_path) then
        os.execute("mkdir -p " .. log_path)
    end

    -- full path
    return log_path .. "/" .. filename
end

---
--- clear log file
---@param file_path string the path of log file
local function clear_log(file_path)
    local file_info = file_info_map[file_path]
    if not file_info then
        file_info = {}
        file_info.fd = io.open(file_path, "w")
        file_info_map[file_path] = file_info
    else
        local fd = file_info.fd
        fd:close()
        file_info.fd = io.open(file_path, "w")
    end

    local fd = file_info.fd
    if fd ~= nil then
        fd:write("")
        fd:close()
    end

    file_info_map[file_path] = nil
end

---
--- write log info
---@param file_path string the path of log file
local function write_log(file_path, ...)
    -- open log file
    local file_info = file_info_map[file_path]
    if not file_info then
        file_info = {}
        file_info.fd = io.open(file_path, "a+")
        file_info_map[file_path] = file_info
    end
    -- adjust log write time
    file_info.write_time = time_helper.get_time()

    local fd = file_info.fd
    if fd ~= nil then
        file_info.has_data = true
        fd:write("-------------[" .. os.date("%Y-%m-%d %X", time_helper.get_time()) .. "]--------------\n")
        local arg = table.pack(...)
        if arg ~= nil then
            for key, value in pairs(arg) do
                if key ~= "n" then
                    if type(value) ~= "table" then
                        fd:write(tostring(value) .. "\n")
                    else
                        fd:write(table_helper.tostring(value) .. "\n")
                    end
                end
            end
        end
    end
end

---
--- log net info
---@param file_path string the path of log file
---@param msg_name string message name
local function write_net_log(file_path, msg_name, ...)
    -- open log file
    local file_info = file_info_map[file_path]
    if not file_info then
        file_info = {}
        file_info.fd = io.open(file_path, "a+")
        file_info_map[file_path] = file_info
    end
    -- adjust log write time
    file_info.write_time = time_helper.get_time()

    local fd = file_info.fd
    if fd ~= nil then
        file_info.has_data = true
        fd:write("[" .. os.date("%Y-%m-%d %X", time_helper.get_time()) .. "] msg_name: " .. msg_name .. "\n")
        local arg = table.pack(...)
        if arg ~= nil then
            for key, value in pairs(arg) do
                if key ~= "n" then
                    if type(value) ~= "table" then
                        fd:write(tostring(value) .. "\n")
                    else
                        fd:write(table_helper.tostring(value) .. "\n")
                    end
                end
            end
        end
    end
end

local CMD = {}

--- start logger service, do initialize
function CMD.start(...)
end

--- exit logger service, do clean
function CMD.exit(...)
    skynet.exit()
end

--- reload logger service
function CMD.reload(...)
end

--- log error
function CMD.error(...)
    local file_path = get_filename(".", "error.log")
    write_log(file_path, ...)
end

--- log info
function CMD.info(...)
    local file_path = get_filename(".", "info.log")
    write_log(file_path, ...)
end

--- log warn
function CMD.warning(...)
    local file_path = get_filename(".", "warning.log")
    write_log(file_path, ...)
end

---
--- log net message
---@param msg_name string message name
function CMD.proto(msg_name, ...)
    -- filter `clientMsg`
    local param = { ... }
    if #param >= 2 and param[2] == "clientMsg" then
        return
    end

    local file_path = get_filename(".", "proto.log")
    write_net_log(file_path, msg_name, ...)
end

---
--- log object data
---@param obj_name string object name, the log filename is generated based on the object name
---@param obj_id string object id, the log filename is generated based on the object id
function CMD.obj(obj_name, obj_id, ...)
    obj_name = obj_name or "."
    obj_id = obj_id or "unknown"

    local file_path = get_filename(obj_name, obj_id .. ".log")
    write_log(file_path, ...)
end

--
skynet.start(function()
    -- set "lua" service message dispatch function
    skynet.dispatch("lua", function(_, address, cmd, ...)
        local f = CMD[cmd]
        if f ~= nil then
            f(...)
        end
    end)

    -- start logger & register service name `.loggerd`
    CMD.start()
    skynet.register(".loggerd")

    -- logger data writer thread
    skynet.fork(function()
        while true do
            -- 3 second
            skynet.sleep(300)

            -- get current seconds
            local now = time_helper.get_time()
            for filename, file_info in pairs(file_info_map) do
                local fd = file_info.fd
                if file_info.has_data then
                    -- flush log data
                    file_info.has_data = false
                    fd:flush()
                elseif file_info.write_time + 3600 < now then
                    -- close log files that have not been written for more than 1 hour
                    fd:close()
                    file_info_map[filename] = nil
                end
            end
        end
    end)
end)

