--
-- time utils
--

local skynet = require "skynet"
local netpack = require "skynet.netpack_ws"

local os = os
local math = math
local tonumber = tonumber

local M = {}

local ONE_MINUTE = 60
local ONE_HOUR = 60 * ONE_MINUTE
local ONE_DAY = 24 * ONE_HOUR
local TIME_ZONE = 8 * 3600

---
--- get current time (seconds, epoch time)
---@return number seconds since the epoch
function M.get_time()
    return math.floor(skynet.time())
end

---
--- get current time (milliseconds, epoch time)
---@return number milliseconds since the epoch
function M.get_time_ms()
    return math.floor(netpack.getms()) -- math.floor(M.get_time() * 1000)
end

---
--- get current time (ticks, 1ticks=10ms, epoch time)
---@return number ticks since the epoch
function M.get_time_ticks()
    return skynet.time() * 100
end

--- get current month
---@return number
function M.get_month()
    return tonumber(os.date("%m", os.time()))
end

--- get current day
---@return number
function M.get_day()
    return tonumber(os.date("%d", os.time()))
end

---
--- get current hour
---@return number
function M.get_hour()
    return tonumber(os.date("%H", os.time()))
end

---
--- which day of the week
---@param weekday number
---@return number
function M.get_weekday(weekday)
    return (weekday + 5) % 7 + 1
end

---
--- diff time
---@param t1
---@param t2
---@return number
function M.get_diff_time(t1, t2)
    return math.floor(os.difftime(t2, t1))
end

---
--- diff days
---@param t1
---@param t2
---@return number
function M.get_diff_days(t1, t2)
    return math.floor((t2 - t1) / ONE_DAY)
end

---
--- is same day
---@param t1
---@param t2
---@return boolean
function M.is_same_day(t1, t2)
    local date_a = os.date("*t", t1)
    local date_b = os.date("*t", t2)
    if date_a.year == date_b.year and date_a.yday == date_b.yday then
        return true
    else
        return false
    end
end

---- 字符串转换时间秒数
---- @param datetimeStr 原始字符串, 格式: "2015-01-11 00:01:40"
---- @param delta 调整值, 对时间进行加减(>0 表示加, <0 表示减)
---- @param deltaUnit 调整单位, DAY,HOUR,MINUTE,SECOND 4种时间单位
---- @return -1表示datetimeStr字符串格式无效
--function M.toTime(datetimeStr, delta, deltaUnit)
--    -- 检查参数
--    if datetimeStr == nil then
--        return -1
--    end
--
--    -- 提取年,月,日,时,分,秒
--    local Y, M, D, H, MM, SS = string.match(datetimeStr, "(%d+)-(%d+)-(%d+) (%d+):(%d+):(%d+)")
--    if Y == nil or M == nil or D == nil or H == nil or MM == nil or SS == nil then
--        return -1
--    end
--
--    -- 转换为time
--    local time = os.time({ year = Y, month = M, day = D, hour = H, min = MM, sec = SS })
--    if delta == nil or deltaUnit == nil then
--        return time
--    end
--
--    -- 根据时间单位和偏移量得到具体的偏移数据
--    local offset = 0
--    if deltaUnit == 'DAY' then
--        offset = ONE_DAY * delta
--    elseif deltaUnit == 'HOUR' then
--        offset = ONE_HOUR * delta
--    elseif deltaUnit == 'MINUTE' then
--        offset = ONE_MINUTE * delta
--    elseif deltaUnit == 'SECOND' then
--        offset = delta
--    end
--
--    return (time + offset)
--end

---
--- convert string to timestamp
---@param time_string string timestamp string, e.g. "12:34:56"
---@return number current timestamp 当前时间的年月日, 时间使用给定时间戳字符串
function M.to_timestamp(time_string)
    local today = os.date('*t', os.time())
    local hour, min, sec = time_string:match("(%d+):(%d+):(%d+)")

    return os.time({
        year = today.year,
        month = today.month,
        day = today.day,
        hour = hour,
        min = min,
        sec = sec
    })
end









--
---- 获取当前时间, 格式: xxxx年xx月xx日xx时xx分
--function M.get_yearmonday_desc()
--    return os.date("%Y年%m月%d日%H时%M分", os.time())
--end
--
---- 获取当前时间, 格式: xxxx年xx月xx日
--function M.get_yearday_desc()
--    return os.date("%Y年%m月%d日", os.time())
--end
--
---- 获取时间串(xxxx_xx_xx)
--function M.get_day_str(timestamp)
--    return os.date("%Y_%m_%d", timestamp)
--end
--
--
--
---- 计算两个时间相差的日期天数（time2 比 time1 晚几个非自然日）
---- @param time1
---- @param time2
---- @param iszone
--function M.get_diffdate_day(time1, time2, iszone)
--    --if iszone ~= nil and not iszone then
--    local date1 = os.date("*t", time1)
--    local date2 = os.date("*t", time2)
--    date1.hour = 0
--    date1.min = 0
--    date1.sec = 0
--    date2.hour = 0
--    date2.min = 0
--    date2.sec = 0
--    time1 = os.time(date1)
--    time2 = os.time(date2)
--    --else
--    --	time1 = (time1+TIME_ZONE)// (3600*24)
--    --	time2 = (time2+TIME_ZONE)// (3600*24)
--    --end
--    return ((time2 - time1) / ONE_DAY)
--end
--
---- 计算自然日时间
---- @param beginTime 为0时取当前时间
---- @param deltaDay 调整天数
--function M.get_day_time(beginTime, deltaDay)
--    if beginTime == 0 then
--        beginTime = M.get_time()
--    end
--
--    -- 格式化
--    local date = os.date("*t", beginTime)
--
--    -- 超过20点, 增加一个自然日
--    if date.hour > 20 then
--        deltaDay = deltaDay + 1
--    end
--
--    --
--    date.hour = 0
--    date.min = 0
--    date.sec = 0
--
--    -- 调整天数
--    return (os.time(date) + deltaDay * ONE_DAY)
--end
--
--
--function M.get_24dian_time()
--    local now = M.get_time()
--    local temp_date = os.date("*t", now)
--
--    temp_date.hour = 23
--    temp_date.min = 59
--    temp_date.sec = 59
--    temp_date.day = temp_date.day
--
--    local time = os.time(temp_date)
--
--    return time
--end
--
---- 获取指定时刻的时间戳(起始和结尾)整点
--function M.get_the_time(start_hour, start_minu, endtime_hour, endtime_minu)
--    local now = M.get_time()
--    local temp_date = os.date("*t", now)
--
--    temp_date.hour = start_hour
--    temp_date.min = start_minu
--    temp_date.sec = 0
--    temp_date.day = temp_date.day
--
--    local time_1 = os.time(temp_date)
--
--    temp_date.hour = endtime_hour
--    temp_date.min = endtime_minu
--    temp_date.sec = 0
--    temp_date.day = temp_date.day
--
--    local time_2 = os.time(temp_date)
--
--    return time_1, time_2
--end
--
----
--function M.getDayTimePoint(upDay)
--    local os_date = os.date("*t")
--    return os.time({ year = os_date.year, month = os_date.month, day = os_date.day, hour = 05, min = 0, sec = 0 }) + 24 * 3600 * upDay
--end
--
----
--function M.getTodaySecond()
--    local now = M.get_time()
--    local temp_date = os.date("*t", now)
--    temp_date.hour = 0
--    temp_date.min = 0
--    temp_date.sec = 0
--    local time1 = os.time(temp_date)
--
--    return now - time1
--end
--
---- 通过小时分钟秒获得当天时间点的时间戳
--function M.getDayPointTimestamp(daydiff, hour, minute, second)
--    local dst = os.time() + daydiff * ONE_DAY
--    local dst_day = os.date("*t", dst)
--    return os.time({ year = dst_day.year, month = dst_day.month, day = dst_day.day, hour = hour, min = minute, sec = second })
--end

return M
