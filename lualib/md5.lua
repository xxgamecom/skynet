----------------------------------------------------------------------------
-- Modify version from https://github.com/keplerproject/md5
----------------------------------------------------------------------------

local md5 = require "md5.core"

----------------------------------------------------------------------------
-- @param k String with original message.
-- @return String with the md5 hash value converted to hexadecimal digits

function md5.sumhexa (k)
    k = md5.sum(k)
    return (string.gsub(k, ".", function(c)
        return string.format("%02x", string.byte(c))
    end))
end

local function get_ipad(c)
    return string.char(c:byte() ~ 0x36)
end

local function get_opad(c)
    return string.char(c:byte() ~ 0x5c)
end

function md5.hmacmd5(data, key)
    if #key > 64 then
        key = md5.sum(key)
        key = key:sub(1, 16)
    end
    local ipad_s = key:gsub(".", get_ipad) .. string.rep("6", 64 - #key)
    local opad_s = key:gsub(".", get_opad) .. string.rep("\\", 64 - #key)
    local istr = md5.sum(ipad_s .. data)
    local ostr = md5.sumhexa(opad_s .. istr)
    return ostr
end

return md5
