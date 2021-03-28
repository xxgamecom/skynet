
local class_super = class("class_super")

function class_super:ctor()
    print("class_super ctor", self)
end

function class_super:init()
    print("class_super init", self)
end

function class_super:foo()
    print("class_super foo", self)
end

return class_super
