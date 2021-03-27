
local class_super = class("class_super")

function class_super:ctor()
    print("class_super ctor")
end

function class_super:init()
    print("class_super init")
end

function class_super:foo()
    print("class_super foo")
end

return class_super
