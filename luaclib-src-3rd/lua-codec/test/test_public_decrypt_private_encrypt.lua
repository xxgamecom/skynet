local codec = require('codec')
local src = '123456'
local pubpem = [[-----BEGIN PUBLIC KEY-----
MIGfMA0GCSqGSIb3DQEBAQUAA4GNADCBiQKBgQDnql6t5/dXQOlbFXXaBBBQc84N
qX3/dU2v792+ZrNPI8maBgc6eXf3oboEYY7fciUkEQ0w4rvyEdu5JmJqZ3iFdEah
UxPsMaWasyTjAesA3+ZH3xgdreFQo+a3a1p2G3MMm0Oa4mVXxySDiSAS1hnjYBJV
th9LoSXKmO6N5WrroQIDAQAB
-----END PUBLIC KEY-----]]


local privpem = [[-----BEGIN PRIVATE KEY-----
MIICdgIBADANBgkqhkiG9w0BAQEFAASCAmAwggJcAgEAAoGBAOeqXq3n91dA6VsV
ddoEEFBzzg2pff91Ta/v3b5ms08jyZoGBzp5d/ehugRhjt9yJSQRDTDiu/IR27km
YmpneIV0RqFTE+wxpZqzJOMB6wDf5kffGB2t4VCj5rdrWnYbcwybQ5riZVfHJIOJ
IBLWGeNgElW2H0uhJcqY7o3lauuhAgMBAAECgYEApCVPWKGX27ceoW8fRg7DEH49
bei+YhdXqGWpFJPoURbmbb//tysCGe/5wcjuVtyl/FwooI7G5MpKiXHtIb+W4H7y
SDeTNW6rSYf0XbrSkr+WAydiVBpIm05R+QL0GNqxvZKC9EwFuUerwcOFiSgdqkWa
MgOSrMz6nn3E2H1o+sECQQD/r06vCMojbvHHwKebZVNQJV8L3xEXDJeJVbS0+Ehu
VOkQZ3AXxUXN1retoTEP3GVQ3XrhXI6phEzYDFsD+sH1AkEA5/N7bS5/p8GyGOVn
C6q3aCTr7TxhwAUYmB9nMphy+Ec4UFqCQTsnW0ClP6MQtrM8dYvq61Dh4k9UNKMv
JDP7fQI/JvOOCRxNrxg3vTacUhAdoRgQYr6Y2+oPK9ziqq8oWaaV2unnKbfj6nfL
g6gK0V/CD4+uKKbxFOIS0tcPBowtAkBJGlzpIUGMbqih3hMnAywAv7o3r9MjALgq
oaMVuCRsCY4/DPeGdY1G3k32i38mBcFlTq7AcWJvwA7K9C9UWqnFAkEA1g3DDVgG
gndxLJtCLTah/G0TqlRB5Lc5pQX7725d54wnPyqqyTnAxu/tAAJ50bbdAshuoMGO
EulaVVsrvOrbgw==
-----END PRIVATE KEY-----
]]


print(src)
local bs = codec.rsa_private_encrypt(src, privpem)
local dst = codec.base64_encode(bs)
print("加密密文", dst)


local dbs = codec.base64_decode(dst)
--print("待解密密文", dbs)
local dsrc = codec.rsa_public_decrypt(dbs, pubpem)

print("明文")
print(dsrc)
--assert(dsrc == src)
