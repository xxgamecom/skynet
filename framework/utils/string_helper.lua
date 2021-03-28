--
-- string utils
--

local math = math
local table = table
local string = string

local M = {}

---
--- is empty
---@param str string
---@return boolean
function M.is_empty(str)
    if str == nil or string.len(str) <= 0 then
        return true
    end

    return false
end

---
--- is not empty
---@param str string
---@return boolean
function M.isNotEmpty(str)
    return not M.is_empty(str)
end

--- truncate space
---@param str string
---@return boolean
function M.trim(str)
    return str:match "^%s*(.-)%s*$"
end

--- divide string by sep
---@param sep string
---@return table
function M.split(str, sep)
    local t = {}

    if type(sep) ~= "string" or string.len(sep) <= 0 then
        return t
    end

    local start = 1
    while true do
        local pos = string.find(str, sep, start, true) -- plain find
        if not pos then
            break
        end
        table.insert(t, string.sub(str, start, pos - 1))
        start = pos + string.len(sep)
    end
    table.insert(t, string.sub(str, start))

    return t
end

---
--- calc string hash
---@param str
---@return string
function M.toHash(str)
    local counter = 1
    local len = string.len(str)
    for i = 1, len, 3 do
        counter = math.fmod(counter * 8161, 4294967279) + -- 2^32 - 17: Prime!
            (string.byte(str, i) * 16776193) +
            ((string.byte(str, i + 1) or (len - i + 256)) * 8372226) +
            ((string.byte(str, i + 2) or (len - i + 256)) * 3932164)
    end
    return math.fmod(counter, 4294967291) -- 2^32 - 5: Prime (and different from the prime in the loop)
end

return M
