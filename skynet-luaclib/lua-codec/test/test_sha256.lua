
local codec = require('codec')
local src = '111232323123231'
local dst = codec.sha256_encode(src)
print(dst)
assert('452aec120b717901f8a117b91211546774c49efd0dc63e2264d284803a20b5fb' == dst)
