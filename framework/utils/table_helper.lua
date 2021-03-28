--
-- table utils
--

local skynet = require("skynet")
local json = require("cjson")

local io = io
local math = math
local print = print
local type = type
local pairs = pairs
local tostring = tostring
local next = next
local table_insert = table.insert
local table_remove = table.remove

local M = {}

-- 设置默认值表
local mt_defaultvalue = {
    __index = function(t)
        return t.__
    end
}
function M.set_default(t, default_value)
    t.__ = default_value
    setmetatable(t, mt_defaultvalue)
end

-- monitor access
local index = {}
local mt_monitor = {
    __index = function(t, k)
        skynet.log("table_helper monitor *access to element " .. tostring(k))
        return t[index][k]
    end,
    __newindex = function(t, k, v)
        skynet.log("table_helper monitor *update to element " .. tostring(k) .. " to " .. tostring(v))
        t[index][k] = v
    end
}
function M.track(t)
    local proxy = {}
    proxy[index] = t
    setmetatable(proxy, mt_monitor)
    return proxy
end

---
--- create a read only table
---@param t table
---@return
function M.create_readonly_table(t)
    local proxy = {}
    local mt = {
        __index = t,
        __newindex = function(t, k, v)
            error("attempt to update a read-only table", 2)
        end
    }

    setmetatable(proxy, mt)
    return proxy
end

---
--- is table empty
---@param t table
---@return boolean
function M.is_empty(t)
    -- nil 或 非table
    if t == nil or type(t) ~= "table" then
        return false
    end

    return (_G.next(t) == nil or M.getn(t) == 0)
end

---
--- is value exists
---@param t table
---@param value
---@return boolean
function M.is_value_exist(t, value)
    if M.is_empty(t) or value == nil then
        return false
    end

    for _, v in pairs(t) do
        if v == value then
            return true
        end
    end

    return false
end

---
--- is key exists
---@param t table
---@param key
---@return boolean
function M.is_key_exist(t, key)
    if M.is_empty(t) or key == nil then
        return false
    end

    for k, _ in pairs(t) do
        if k == key then
            return true
        end
    end

    return false
end

---
--- get the number of table
---@param t table
---@return number
function M.getn(t)
    local count = 0
    for _, _ in pairs(t) do
        count = count + 1
    end
    return count
end

---
--- convert to string
---@param tab table
---@return
function M.tostring(tab)
    local cache = { [tab] = "." }
    local function _dump(t, space, name)
        local temp = {}
        for k, v in pairs(t) do
            local key = tostring(k)
            if cache[v] then
                table_insert(temp, "+" .. key .. " {" .. cache[v] .. "}")
            elseif type(v) == "table" then
                local new_key = name .. "." .. key
                cache[v] = new_key
                table_insert(temp, "+" .. key .. _dump(v, space .. (next(t, k) and "|" or " ") .. string.rep(" ", #key), new_key))
            else
                table_insert(temp, "+" .. key .. " [" .. tostring(v) .. "]")
            end
        end
        return table.concat(temp, "\n" .. space)
    end
    return _dump(tab, "", "")
end

---
--- deep copy
---@param obj
function M.deepcopy(obj)
    local lookup_table = {}
    local function _copy(obj)
        if type(obj) ~= "table" then
            return obj
        elseif lookup_table[obj] then
            return lookup_table[obj]
        end

        local new_table = {}
        lookup_table[obj] = new_table
        for index, value in pairs(obj) do
            new_table[_copy(index)] = _copy(value)
        end

        return new_table
        --return setmetatable(new_table, _copy(getmetatable(obj)))
    end
    return _copy(obj)
end

---
--- print table
---@param root
function M.print_r(root)
    local cache = { [root] = "." }
    local function _dump(t, space, name)
        local temp = {}
        for k, v in pairs(t) do
            local key = tostring(k)
            if cache[v] then
                table_insert(temp, "+" .. key .. " {" .. cache[v] .. "}")
            elseif type(v) == "table" then
                local new_key = name .. "." .. key
                cache[v] = new_key
                table_insert(temp, "+" .. key .. _dump(v, space .. (next(t, k) and "|" or " ") .. string.rep(" ", #key), new_key))
            else
                table_insert(temp, "+" .. key .. " [" .. tostring(v) .. "]")
            end
        end
        return table.concat(temp, "\n" .. space)
    end
    print(_dump(root, "", ""))
end

---
--- delete table element
---@param list table
---@param item
---@param is_remove_all
function M.remove_item(list, item, is_remove_all)
    local rmcount = 0
    for i = 1, #list do
        if list[i - rmcount] == item then
            table_remove(list, i - rmcount)
            if is_remove_all then
                rmcount = rmcount + 1
            else
                break
            end
        end
    end
end

---
--- write table to file
---@param fd
---@param obj
---@param index
---@param flag
local function write_to_file(fd, obj, index, flag)
    local szType = type(obj)
    if szType == "number" then
        fd:write(obj)
    elseif szType == "string" then
        fd:write(string.format("%q", obj))
    elseif szType == "table" then
        -- 把table的内容格式化写入文件
        index = index + 1
        local spaces = ""
        for i = 1, index do
            spaces = spaces .. " "
        end
        if flag == false then
            fd:write(spaces .. "{\n")
        else
            fd:write("{\n")
        end
        for i, v in pairs(obj) do
            if type(i) ~= "number" then
                fd:write(spaces .. " ")
                fd:write(i)
                fd:write("=")
                write_to_file(fd, v, index, true)
            else
                write_to_file(fd, v, index, false)
            end
            if type(v) ~= "table" then
                fd:write(", \n")
            end
        end
        if index > 1 then
            fd:write(spaces .. "},\n")
        else
            fd:write(spaces .. "}\n")
        end
    else
        skynet.log("can't serialize, type " .. szType)
    end

    if index > 1 then
        index = index - 1
    end
end

---
--- save table to file
---@param filename
---@param obj
---@return
function M.save_object(filename, obj)
    if filename == nil or obj == nil then
        return
    end

    local fd = io.open(filename, "w+")
    if not fd then
        return
    end

    local index = 0
    local flag = false
    fd:write("return \n")
    write_to_file(fd, obj, index, flag)

    fd:close()
end

---
--- serialize to json object
---@param t table
---@return
function M.serialize(t)
    return json.encode(t)
end

---
--- deserialize from json object
---@return
function M.deserialize(...)
    return json.decode(...)
end

---
--- random_table
---@param t table
---@return
function M.random_table(t)
    if type(t) ~= "table" then
        return
    end

    local l = #t
    local tab = {}
    local index = 1
    while #t ~= 0 do
        local n = math.random(0, #t)
        if t[n] ~= nil then
            tab[index] = t[n]
            table_remove(t, n)
            index = index + 1
        end
    end
    return tab
end

return M

