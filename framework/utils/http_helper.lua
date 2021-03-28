--
-- http utils
--

local curl = require "luacurl"

local string = string
local table = table
local tonumber = tonumber

local M = {}

---
--- check whether the url starts with https://
---@param url
---@return boolean
function M.is_https(url)
    return string.find(url, "https://")
end

---
--- check whether the url starts with http://
---@param url
---@return boolean
function M.is_http(url)
    return string.find(url, "http://")
end

---
--- get the host in the url
---@param url
---@return string
function M.get_host(url)
    --
    local n
    local start_pos = 0
    local end_pos = 0
    if M.is_https(url) then
        n, start_pos = string.find(url, "https://")
    elseif M.is_http(url) then
        n, start_pos = string.find(url, "http://")
    else
        return ""
    end
    n, end_pos = string.find(url, "/", start_pos + 1)

    return string.sub(url, start_pos + 1, end_pos - 1)
end

---
--- url encode
---@param url
---@return string
function M.url_encode(url)
    url = string.gsub(url, "([^%w%.%- ])", function(c)
        return string.format("%%%02X", string.byte(c))
    end)
    return string.gsub(url, " ", "+")
end

---
--- url decode
---@param url
---@return string
function M.url_decode(url)
    url = string.gsub(url, '%%(%x%x)', function(h)
        return string.char(tonumber(h, 16))
    end)
    return url
end

---
--- send http get request
---@param url string request url
---@param timeout number request timeout, 0 or nil return default 10 seconds
---@return number, string status and result
function M.http_get(url, timeout)
    if timeout == nil or timeout == 0 then
        timeout = 10
    end

    local result = {}
    local c = curl.new()
    c:setopt(curl.OPT_URL, url)
    c:setopt(curl.OPT_WRITEDATA, result)
    c:setopt(curl.OPT_WRITEFUNCTION, function(tab, buffer)
        -- callback函数，必须有
        table.insert(tab, buffer) -- tab参数即为result，参考http://luacurl.luaforge.net/
        return #buffer
    end)
    c:setopt(curl.OPT_TIMEOUT, timeout)
    local status = c:perform()
    c:close()

    return status, table.concat(result)
end

---
--- send http post request
---@param url string request url
---@param content string post body
---@param timeout number request timeout, 0 or nil return default 10 seconds
---@return number, string status and result
function M.http_post(url, content, timeout)
    if timeout == nil or timeout == 0 then
        timeout = 10
    end

    local result = {}
    local c = curl.new()
    c:setopt(curl.OPT_URL, url)
    c:setopt(curl.OPT_POST, true)
    c:setopt(curl.OPT_POSTFIELDS, content)
    c:setopt(curl.OPT_WRITEDATA, result)
    c:setopt(curl.OPT_WRITEFUNCTION, function(tab, buffer)
        -- callback函数，必须有
        table.insert(tab, buffer) -- tab参数即为result，参考http://luacurl.luaforge.net/
        return #buffer
    end)
    c:setopt(curl.OPT_TIMEOUT, timeout)
    local status = c:perform()
    c:close()

    return status, table.concat(result)
end

return M

