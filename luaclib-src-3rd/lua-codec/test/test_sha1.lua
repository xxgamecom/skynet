local codec = require('codec')
local src = '123213123123123'
local dst = codec.sha1_encode(src)
print(dst)
assert('0cc93f98e751ca3e1e43b8746768d28837678cbe' == dst)
