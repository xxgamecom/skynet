require("class")

local class_sub = require("class.class_sub")

local sub = class_sub.create()
sub:init()
sub:foo()

print("is kind of class_sub", iskindof(sub, "class_sub"))
print("is kind of class_super", iskindof(sub, "class_super"))
