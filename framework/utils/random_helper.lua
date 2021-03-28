--
-- random utils
--

local os = os
local io = io
local math = math
local string = string
local tostring = tostring
local table = table

local M = {

    -- random mode
    RandomModel = {
        RSM_Capital = 1, -- upper-case letters only
        RSM_Letter = 2, -- lower-case letters only
        RSM_Cap_Let = 3, -- upper-case and lower-case letters
        RSM_Number = 4, -- numbers only
        RSM_Cap_Num = 5, -- upper-case and letters
        RSM_Let_Num = 6, -- lower-case and letters
        RSM_ALL = 7, -- numbers and upper and lower case letters
    },

}

local function do_random_string(n, m)
    -- 构造自己的随机函数: 思路是用随机结果放大处理以后再置随机种子，然后随机
    math.randomseed(os.clock() * math.random(1000000, 90000000) + math.random(1000000, 90000000))
    return math.random(n, m)
end

---
--- random string
---@param len number
---@return string
function M.random_number(len)
    local rt = ""
    for i = 1, len, 1 do
        if i == 1 then
            rt = rt .. do_random_string(1, 9)
        else
            rt = rt .. do_random_string(0, 9)
        end
    end
    return rt
end

---
--- random lower-case letters
---@param len number
---@return string
function M.random_letter(len)
    local rt = ""
    for i = 1, len, 1 do
        rt = rt .. string.char(do_random_string(97, 122))
    end
    return rt
end

---
--- random upper-case letters
---@param len number
---@return string
function M.random_capital(len)
    local rt = ""
    for i = 1, len, 1 do
        rt = rt .. string.char(do_random_string(65, 90))
    end
    return rt
end

local BC = "ABCDEFGHIJKLMNOPQRSTUVWXYZ" -- 1
local SC = "abcdefghijklmnopqrstuvwxyz" -- 2
local NO = "0123456789"    -- 4

---
--- random string
---@param len number string length
---@param model number random mode, default generate upper-case letters
---@return string
function M.random_string(len, model)
    local max_len = 0
    local template = ""
    if model == nil then
        -- no model, default generate upper-case letters
        template = BC
        max_len = 26
    elseif model == M.RandomModel.RSM_Capital then
        -- only include upper-case letters
        template = BC
        max_len = 26
    elseif model == M.RandomModel.RSM_Letter then
        -- only include lower-case letters
        template = SC
        max_len = 26
    elseif model == M.RandomModel.RSM_Cap_Let then
        -- include upper-case and lower-case letters
        template = SC .. BC
        max_len = 52
    elseif model == M.RandomModel.RSM_Number then
        -- numbers only
        template = NO
        max_len = 10
    elseif model == M.RandomModel.RSM_Cap_Num then
        -- upper-case letters and numbers
        template = NO .. BC
        max_len = 36
    elseif model == M.RandomModel.RSM_Let_Num then
        -- lower-case letters and numbers
        template = NO .. SC
        max_len = 36
    elseif model == M.RandomModel.RSM_ALL then
        -- numbers and upper and lower case letters
        template = NO .. SC .. BC
        max_len = 62
    else
        -- generate upper-case letters
        template = BC
        max_len = 26
    end

    -- generate
    local srt = {}
    for i = 1, len, 1 do
        local index = do_random_string(1, max_len)
        srt[i] = string.sub(template, index, index)
    end
    return table.concat(srt, "")
end

-- random seed initialize flag
local random_seed_inited = 0

---
--- random number in range
---@param min number min range
---@param max number max range
---@return number
function M.random(min, max)
    if random_seed_inited == 0 then
        math.randomseed(tostring(os.time()):reverse():sub(1, 6))
    end
    random_seed_inited = 1

    return math.random(min, max)
end

---
--- random number, use /dev/urandom
---@return string
function M.RNG()
    local status, urand = pcall(io.open, '/dev/urandom', 'rb')
    if not status then
        return nil
    end

    local b = 4
    local m = 256
    local n, s = 0, urand:read(b)
    for i = 1, s:len() do
        n = m * n + s:byte(i)
    end

    io.close(urand)

    return n
end

---
--- random table
---@param tab table
---@return table
function M.random_table(tab)
    if type(tab) ~= "table" then
        return nil
    end

    local new_tab = {}
    local index = 1
    while #tab ~= 0 do
        local n = math.random(0, #tab)
        if tab[n] ~= nil then
            new_tab[index] = tab[n]
            table.remove(tab, n)
            index = index + 1
        end
    end

    return new_tab
end

---
--- generate uuid
---@return string
function M.gen_uuid()
    local template = "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
    local status, urand = pcall(io.open, "/dev/urandom", "r")
    if not status then
        return nil
    end

    local d = urand:read(4)
    math.randomseed(os.time() + d:byte(1) + (d:byte(2) * 256) + (d:byte(3) * 65536) + (d:byte(4) * 4294967296))
    local uuid = string.gsub(template, "x", function(c)
        local v = (c == "x") and math.random(0, 0xf) or math.random(8, 0xb)
        return string.format("%x", v)
    end)

    io.close(urand)

    return uuid
end

return M
