
local codec = require"codec"
require"dump"

local paystr = [[sign=s6grVWOcSRtWd9PQS65TdWSHjNAGp01mDN1bteGZiunKZbx7qiPlBRqlsrVr7yCLMeJE6a7L7MNzbs%2FoGNYbtwWKNZ9YBY7MZqDAfWpIYfa8e88TdB6NcbSNPpmvSxclzl59Jt4%2FkhzafdWx8zNgZiLlkufeECK9GWzMNRtGZ5U%3D&uid=s5a3c69d2afeaf&v=1.0&notify_data=Ltz5CgHljNSVmQO7lNi61FyZoDQ9WrhQClZIuOj4SqElW5dGEV%2FN3zqznBxiiMjFcl7sG0mbn%2BvYgevuBdNGOQjoBQnFTNjqmt3ETDTJkTol3RPAi%2BUaQ38UqaDP7fqYbl96bP3tgA47lftRFN0fUiAvKFYoiz8KvCOcMxt0j10%3D&dealseq=b90647362db09023f3f92c1de7e6f900&subject=10%E9%92%BB%E7%9F%B3&orderid=17122210524819330401967]]


local public_key_pem = [[-----BEGIN PUBLIC KEY-----
MIGfMA0GCSqGSIb3DQEBAQUAA4GNADCBiQKBgQC6nMYyAzcmKHG1wM1mkkDZBAww
AkfnBtuoFukNJtwxWgXmfwG+rgupy/d5FHqQxZb8vlbnKHqsjue3O0Mt/gMUsc/E
84xUJWDMtVRUkD//W6uv0d+t5++NGDHddLDqqfWdb0pK+8RYt9Lpvm+h6IXmBEUL
mMzq1TX/z5wu8TlT/wIDAQAB
-----END PUBLIC KEY-----]]


local function strsplit(str, delim)
    if type(delim) ~= "string" or string.len(delim) <= 0 then
        return
    end 
    local start = 1
    local t = {}
    while true do
        local pos = string.find (str, delim, start, true) -- plain find
        if not pos then
            break
        end
        table.insert (t, string.sub (str, start, pos - 1))
        start = pos + string.len (delim)
    end
    table.insert (t, string.sub (str, start))
    return t
end



local function decodePostResult(result, split)
    local t = strsplit(result, split)
    local ret = {}
    for i, v in ipairs(t) do
        local tmp = strsplit(v, "=")
        ret[tmp[1]] = tmp[2]
    end
    return ret
end



local function urlDecode(s) 
    s = string.gsub(s, '%%(%x%x)', function(h) return string.char(tonumber(h, 16)) end) 
    return s 
end 


local data = decodePostResult(paystr, "&")

for k, v in pairs(data) do
    data[k] = urlDecode(v)
end


table.sort(data, function(a, b)
    return a > b 
end)

dump(data, "解析")


local originStr = ""

local order = {"dealseq", "notify_data", "orderid", "subject", "uid", "v"}


for _, key in ipairs(order) do
    for k, v in pairs(data) do
        if k == key then
            if string.len(originStr) > 0 then
                originStr = originStr .. "&" ..k.."="..v
            else
                originStr = originStr ..k.."="..v
            end
        end
    end
end

print(originStr)

local sign = codec.base64_decode(data.sign)

--print("sign", sign)

--local ok = codec.rsa_kuaiyong_verify(originStr, sign, public_key_pem)

local ok = codec.rsa_public_verify(originStr, sign, public_key_pem, 2)

print("ok", ok)



local notify_data = codec.base64_decode(data.notify_data)

--print("notify_data", notify_data)

local notify = codec.rsa_public_decrypt(notify_data, public_key_pem)

print("notify", notify)

local t = decodePostResult(notify, "&")

dump(t)


















