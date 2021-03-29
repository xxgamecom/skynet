# Object-Oriented support

## 1 class method
```lua
class.create()  -- instance factory
class.new()     -- instance factory
class.ctor()    -- constructor function
```

## 2 helper function
```lua
iskindof()      -- is kind of class
```

## 3 examples

### class_super.lua
```lua
-- define super class
local class_super = class("class_super")

-- constructor
function class_super:ctor()
    print("class_super ctor")
end

function class_super:init()
    print("class_super init")
end

function class_super:foo()
    print("class_super foo")
end
```

### class_sub.lua 
```lua
-- define sub class
local class_sub = class("class_sub", require("class_super"))

-- override
function class_sub:foo()
    -- call super
    class_sub.super:foo()
    --
    print("class_sub foo")
end

```
