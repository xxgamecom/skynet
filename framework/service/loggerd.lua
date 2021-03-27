--
-- logger service
--

local skynet = require "skynet"
require "skynet.manager"

local FileUtils = require "utils.FileUtils"
local TimeUtils = require "utils.TimeUtils"
local TableUtils = require "utils.TableUtils"

-- 文件信息表
local fileInfoMap = {}

-- 获取日志文件名
local function getFilename(dirname, filename)
    -- 日志分日期存储
    local path = skynet.getenv("logpath")
    if path == nil then
        path = "."
    end
    if dirname == nil then
        dirname = "."
    end

    -- 创建日志文件夹
    local currentTime = os.date("%Y_%m_%d", TimeUtils.getTime())
    local logPath = path .. "/" .. currentTime .. "/" .. dirname
    if not FileUtils.isFileExists(logPath) then
        os.execute("mkdir -p " .. logPath)
    end

    -- 返回日志文件名完整路径
    return logPath .. "/" .. filename
end

-- 清理日志文件
-- @param filePath 日志文件路径
local function clearLog(filePath)
    local fileInfo = fileInfoMap[filePath]
    if not fileInfo then
        fileInfo = {}
        fileInfo.fileHandle = io.open(filePath, "w")
        fileInfoMap[filePath] = fileInfo
    else
        local fileHandle = fileInfo.fileHandle
        fileHandle:close()
        fileInfo.fileHandle = io.open(filePath, "w")
    end

    local fileHandle = fileInfo.fileHandle
    if fileHandle ~= nil then
        fileHandle:write("")
        fileHandle:close()
    end

    fileInfoMap[filePath] = nil
end

-- 写普通日志
-- @param filePath 日志文件路径
local function writeLog(filePath, ...)
    -- 打开日志文件
    local fileInfo = fileInfoMap[filePath]
    if not fileInfo then
        fileInfo = {}
        fileInfo.fileHandle = io.open(filePath, "a+")
        fileInfoMap[filePath] = fileInfo
    end
    -- 调整日志写入时间
    fileInfo.writeTime = TimeUtils.getTime()

    local fileHandle = fileInfo.fileHandle
    if fileHandle ~= nil then
        fileInfo.hasData = true
        fileHandle:write("-------------[" .. os.date("%Y-%m-%d %X", TimeUtils.getTime()) .. "]--------------\n")
        local arg = table.pack(...)
        if arg ~= nil then
            for key, value in pairs(arg) do
                if key ~= "n" then
                    if type(value) ~= "table" then
                        fileHandle:write(tostring(value) .. "\n")
                    else
                        fileHandle:write(TableUtils.tostring(value) .. "\n")
                    end
                end
            end
        end
    end
end

-- 写协议日志
-- @param filePath 日志文件路径
-- @param msgName 消息名
local function writeProtoLog(filePath, msgName, ...)
    -- 打开日志文件
    local fileInfo = fileInfoMap[filePath]
    if not fileInfo then
        fileInfo = {}
        fileInfo.fileHandle = io.open(filePath, "a+")
        fileInfoMap[filePath] = fileInfo
    end
    -- 调整日志写入时间
    fileInfo.writeTime = TimeUtils.getTime()

    local fileHandle = fileInfo.fileHandle
    if fileHandle ~= nil then
        fileInfo.hasData = true
        fileHandle:write("[" .. os.date("%Y-%m-%d %X", TimeUtils.getTime()) .. "] msgName: " .. msgName .. "\n")
        local arg = table.pack(...)
        if arg ~= nil then
            for key, value in pairs(arg) do
                if key ~= "n" then
                    if type(value) ~= "table" then
                        fileHandle:write(tostring(value) .. "\n")
                    else
                        fileHandle:write(TableUtils.tostring(value) .. "\n")
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
    local filePath = getFilename(".", "error.log")
    writeLog(filePath, ...)
end

-- 写信息日志
function CMD.info(...)
    local filePath = getFilename(".", "info.log")
    writeLog(filePath, ...)
end

-- 写警告日志
function CMD.warning(...)
    local filePath = getFilename(".", "warning.log")
    writeLog(filePath, ...)
end

-- 写通信协议日志
function CMD.proto(msgName, ...)
    -- 过滤掉 clientMsg
    local param = { ... }
    if #param >= 2 and param[2] == "clientMsg" then
        return
    end

    local filePath = getFilename(".", "proto.log")
    writeProtoLog(filePath, msgName, ...)
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

    local filePath = getFilename(objName, objId .. ".log")
    writeLog(filePath, ...)
end

-- ----------------------------------------------
-- service function
-- ----------------------------------------------

-- 服务入口
skynet.start(function()
    -- 注册lua消息处理
    skynet.dispatch("lua", function(_, address, cmd, ...)
        local f = CMD[cmd]
        if f ~= nil then
            f(...)
        end
    end)

    --
    CMD.start()
    skynet.register(".loggerd")

    -- 日志数据落地协程
    skynet.fork(function()
        while true do
            -- 3 second
            skynet.sleep(300)

            local now = TimeUtils.getTime()
            for filename, fileInfo in pairs(fileInfoMap) do
                local fileHandle = fileInfo.fileHandle
                if fileInfo.hasData then
                    -- 日志数据落地
                    fileInfo.hasData = false
                    fileHandle:flush()
                elseif fileInfo.writeTime + 3600 < now then
                    -- 关闭超过1小时没有数据写入的日志文件
                    fileHandle:close()
                    fileInfoMap[filename] = nil
                end
            end
        end
    end)
end)

