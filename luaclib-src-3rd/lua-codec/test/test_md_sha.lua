local codec = require('codec')
local sign ='ec+8sy8Ps3vSbRFNsz5cV/v+5t/KPp1KaFDV8/8kXBALbRZ7bj3m8gXryaRedsn7+bpSSHSU/m6bPHHwmotPBxeHk35eLfT/cHZTbY63oFLtMBw88L6kxMuFsIYc9qGHZUvpWrwH4TB8XRwFHNTr43qrAqpbhIXCmEo+m9oi2NEEjMEkMU5x9DZIuLrlaKCdlk30HV0moqoo0/uGLfJOf7Z9kyRsetMoCju6UIkK+aGqSHLPfu4PikwlGdPpBOwB8U4t2XkBxOjieP8XH6ZRX4fBqFpk7Q1srQpueq4F5r5ASDAwkSmbkiTgE8OYkJaF3LZUNMTjJ6ltaP1nlEhFSQ=='
local content = 'opay_status=0&order_type=2708&out_trade_no=153554746210034060&pay_status=1&return_code=SUCCESS&return_msg=order_notify&seller_id=561713247&state=00&total_fee=2000'
local key = [[-----BEGIN PUBLIC KEY-----
MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEA0tU8imsKRK3J5nCT1PQu
ZujwhykJP6TOZZAZc6qSW0CSqFooQaO6XPQl2lmRMknfR83SYPjphDpUaqlKxswY
Y3DO/D9oj9lRATMwou3HKLUJ6lsQ+VeCDx01lq0OaGHRaTb2R3r3wc+IbleQnphS
T8Au7/ZDfEYnnK9qCSMcqBZhDAamyBQa5ykRqX/BRsOL9mwk0v39mZewAt4tsvae
lWJu6E+KY5guZQtib4q8kzEzT3amO0WOV5/c5SdG0MBkc+XE9Wcb6JOjocdG9yCN
nVVh0NEjrGB7qs3bq0zvQ89dajZIh8yHPIQlnCKwIB3XCVKvR2/RjbhHSeYdmHuX
hQIDAQAB
-----END PUBLIC KEY-----]]

local ok = codec.rsa_md5_sha1_verify(content,sign,key,2)

print("验签结果",ok)
