require("class")

local class_sub = require("class.class_sub")

--
print("create sub")
local sub = class_sub.create()

print("call class_sub")
--
class_sub:init()
class_sub:foo()

print("is kind of class_sub", iskindof(sub, "class_sub"))
print("is kind of class_super", iskindof(sub, "class_super"))
