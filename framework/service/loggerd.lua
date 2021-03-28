--
-- logger service
--

local skynet = require "skynet"
require "skynet.manager"

local file_helper = require "utils.file_helper"
local time_helper = require "utils.time_helper"
local table_helper = require "utils.table_helper"

local file_info_map = {}    -- key: file path, value: file info ({ fd, has_data, write_time })

---
--- get log filename
---@param dirname
---@param filename
---@return string the full path of log
local function get_filename(dirname, filename)
    -- store divide by date
    local path = skynet.getenv("log_path")
    if path == nil then
        path = "."
    end
    if dirname == nil then
        dirname = "."
    end

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
---@param file_path the path of log file
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
---@param file_path the path of log file
local function write_log(file_path, ...)
    -- 打开日志文件
    local file_info = file_info_map[file_path]
    if not file_info then
        file_info = {}
        file_info.fd = io.open(file_path, "a+")
        file_info_map[file_path] = file_info
    end
    -- 调整日志写入时间
    file_info.writeTime = time_helper.get_time()

    local fd = file_info.fd
    if fd ~= nil then
        file_info.hasData = true
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
---@param file_path 日志文件路径
---@param msgName 消息名
local function writeProtoLog(file_path, msgName, ...)
    -- 打开日志文件
    local file_info = file_info_map[file_path]
    if not file_info then
        file_info = {}
        file_info.fd = io.open(file_path, "a+")
        file_info_map[file_path] = file_info
    end
    -- 调整日志写入时间
    file_info.writeTime = time_helper.get_time()

    local fd = file_info.fd
    if fd ~= nil then
        file_info.hasData = true
        fd:write("[" .. os.date("%Y-%m-%d %X", time_helper.get_time()) .. "] msgName: " .. msgName .. "\n")
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

function CMD.start(...)
end

function CMD.exit(...)
    skynet.exit()
end

function CMD.reload(...)
end

-- 写错误日志
function CMD.error(...)
    local file_path = get_filename(".", "error.log")
    write_log(file_path, ...)
end

-- 写信息日志
function CMD.info(...)
    local file_path = get_filename(".", "info.log")
    write_log(file_path, ...)
end

-- 写警告日志
function CMD.warning(...)
    local file_path = get_filename(".", "warning.log")
    write_log(file_path, ...)
end

-- 写通信协议日志
function CMD.proto(msgName, ...)
    -- 过滤掉 clientMsg
    local param = { ... }
    if #param >= 2 and param[2] == "clientMsg" then
        return
    end

    local file_path = get_filename(".", "proto.log")
    writeProtoLog(file_path, msgName, ...)
end

-- 写对象日志
-- @param objName, 对象名称, 会根据该对象名称创建对象日志目录, 将对象日志写到该名称的目录下
-- @param objId, 对象ID, 会根据该对象ID生成日志文件名
function CMD.obj(objName, objId, ...)
    if objName == nil then
        objName = "."
    end
    if objId == nil then
        objId = "unknown"
    end

    local file_path = get_filename(objName, objId .. ".log")
    write_log(file_path, ...)
end

-- ----------------------------------------------
-- service function
-- ----------------------------------------------

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

            local now = time_helper.get_time()
            for filename, file_info in pairs(file_info_map) do
                local fd = file_info.fd
                if file_info.hasData then
                    -- 日志数据落地
                    file_info.hasData = false
                    fd:flush()
                elseif file_info.writeTime + 3600 < now then
                    -- 关闭超过1小时没有数据写入的日志文件
                    fd:close()
                    file_info_map[filename] = nil
                end
            end
        end
    end)
end)

