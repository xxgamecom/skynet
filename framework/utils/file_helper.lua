--
-- file utils
--

local lfs = require "lfs"

local table_insert = table.insert

local M = {}

---
--- check file exists
---@param path string file path
---@return boolean
function M.is_exists(path)
    local fd = io.open(path, "rb")
    if fd then
        fd:close()
    end
    return fd ~= nil
end

---
--- get file attribute
---@param dir string file path
---@return
function M.get_file_attribute(dir)
    if dir == nil then
        return nil, nil
    end

    local file_attr = lfs.attributes(dir)
    if not file_attr then
        return nil, nil
    end

    return (file_attr), file_attr
end

---
--- get file size
---@param dir string file path
---@return
function M.get_file_size(dir)
    if dir == nil then
        return false, nil
    end

    local file_attr = lfs.attributes(dir)
    if not file_attr then
        return false, nil
    end

    return true, file_attr.size
end

---
--- get file modify time
---@param dir string file path
---@return
function M.get_file_change_time(dir)
    if dir == nil then
        return false, nil
    end

    local file_attr = lfs.attributes(dir)
    if not file_attr then
        return false, nil
    end

    return true, file_attr.modification
end

---
--- get all filenames in the directory
---@param dir string file path
---@return table
function M.get_all_filenames(dir)
    local filenames = {}
    for filename in lfs.dir(dir) do
        -- ignore . & ..
        if filename ~= "." and filename ~= ".." then
            -- only process file
            local f = dir .. '/' .. filename
            local attr = lfs.attributes(f)
            if attr.mode ~= "directory" then
                table_insert(filenames, filename)
            end
        end
    end

    return filenames
end

return M
