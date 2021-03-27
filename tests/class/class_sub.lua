local class_sub = class("class_sub", require("class.class_super"))

function class_sub:ctor()
    class_sub.super:ctor()

    print("class_sub ctor")
end

-- override
function class_sub:foo()
    -- call super
    class_sub.super:foo()

    --
    print("class_sub foo")
end

return class_sub
