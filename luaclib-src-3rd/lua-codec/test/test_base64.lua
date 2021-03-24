local codec = require('codec')
local src = '123456'
local dst = codec.base64_encode(src)
print(dst)
local dsrc = codec.base64_decode(dst)
print(dsrc)
assert(dsrc == src)


local str =[[BozVKjcmmlEIwW+wr6BfOaYxWvMwX/lSlZKSX7jwvLiJqnfo/HcKsHtqQQT15yk6L9pAuCUpe2GbzZz
6e9CS1j8VYzOoiaypIrdGC4bApQaHtWTHGzBhP1WG7bbSL5zAiWZgSMvglIR2UHzS99yf8fwzt2D2JFjT/pYcuXsJu3O
A0ASYYKz/N0xDF6ot/Ib8Niu3psdtIwO0Zny/dNwaPD0j6okKU7EXKZ0ZRXFRzuQCHtSKxEI5tlSIu8cigDN8awjDjAL
IFxH8IlF0AzpdCfLSq6mhKXLM/wexyRoPXObJycdkpaRuUvxUePFqzYyf3wxH6Wy8yuFM6RpV8EaQZhoJhYDwA8jKK08
mC3kwSbX1DQOqE5ZXzdcY010E1Z/GweCNPM8p3woEkXED+p41sxrTPICLkujB53UkMtEhDSZeLD353BCk8aAa/0RkfhY
n8n5kfFni7h/WYcg544UTa9Y82D37cvVJp+zhRkZKdpxeuJGRkF58Lz8c/ibjSu6n]]

local str2 = codec.base64_decode(str)
print("str2",#str2,str2)
local str2 = codec.base64_decode_all(str)
print("str2",#str2,str2)