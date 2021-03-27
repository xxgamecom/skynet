--
-- Object-Oriented support
--
-- 1 class method
-- class.create()  -- instance factory
-- class.new()     -- instance factory
-- class.ctor()    -- constructor function
--
-- 2 helper function
-- iskindof()      -- is kind of class
--
-- 3 examples
--
-- class_super.lua
--
-- -- define super class
-- local class_super = class("class_super")
--
-- -- constructor
-- function class_super:ctor()
--     print("class_super ctor")
-- end
--
-- function class_super:init()
--     print("class_super init")
-- end
--
-- function class_super:foo()
--     print("class_super foo")
-- end
--
-- class_sub.lua
--
-- -- define sub class
-- local class_sub = class("class_sub", require("class_super"))
--
-- -- override
-- function class_sub:foo()
--     -- call super
--     class_sub.super:foo()
--     --
--     print("class_sub foo")
-- end
--

local skynet_oo = require("skynet.oo")

local type = type
local ipairs = ipairs
local rawget = rawget
local getmetatable = getmetatable
local setmetatable = setmetatable

local _set_metatable_index
_set_metatable_index = function(t, index)
    if type(t) == "userdata" then
        local peer = skynet_oo.getpeer(t)
        if not peer then
            peer = {}
            skynet_oo.setpeer(t, peer)
        end
        _set_metatable_index(peer, index)
    else
        local mt = getmetatable(t)
        if not mt then
            mt = {}
        end
        if not mt.__index then
            mt.__index = index
            setmetatable(t, mt)
        elseif mt.__index ~= index then
            _set_metatable_index(mt, index)
        end
    end
end

---
--- define a class
--- @param classname string class name
--- @param ... any
function class(classname, ...)
    local cls = { __cname = classname }

    local supers = { ... }
    for _, super in ipairs(supers) do
        local superType = type(super)
        assert(superType == "nil" or superType == "table" or superType == "function",
            string.format("class() - create class \"%s\" with invalid super class type \"%s\"",
                classname, superType))

        if superType == "function" then
            assert(cls.__create == nil,
                string.format("class() - create class \"%s\" with more than one creating function", classname)
            );
            -- if super is function, set it to __create
            cls.__create = super
        elseif superType == "table" then
            if super[".isclass"] then
                -- super is native class
                assert(cls.__create == nil,
                    string.format("class() - create class \"%s\" with more than one creating function or native class", classname)
                );
                cls.__create = function()
                    return super:create()
                end
            else
                -- super is pure lua class
                cls.__supers = cls.__supers or {}
                cls.__supers[#cls.__supers + 1] = super
                if not cls.super then
                    -- set first super pure lua class as class.super
                    cls.super = super
                end
            end
        else
            error(string.format("class() - create class \"%s\" with invalid super type",
                classname), 0)
        end
    end

    cls.__index = cls
    if not cls.__supers or #cls.__supers == 1 then
        setmetatable(cls, { __index = cls.super })
    else
        setmetatable(cls, { __index = function(_, key)
            local supers = cls.__supers
            for i = 1, #supers do
                local super = supers[i]
                if super[key] then
                    return super[key]
                end
            end
        end })
    end

    -- add default constructor
    if not cls.ctor then
        cls.ctor = function()
        end
    end
    -- instance factory
    cls.new = function(...)
        local instance
        if cls.__create then
            instance = cls.__create(...)
        else
            instance = {}
        end
        _set_metatable_index(instance, cls)
        instance.class = cls
        instance:ctor(...)
        return instance
    end
    cls.create = function(_, ...)
        return cls.new(...)
    end

    return cls
end

local _iskindof
_iskindof = function(cls, name)
    local __index = rawget(cls, "__index")
    if type(__index) == "table" and rawget(__index, "__cname") == name then
        return true
    end

    if rawget(cls, "__cname") == name then
        return true
    end
    local __supers = rawget(__index, "__supers")
    if not __supers then
        return false
    end
    for _, super in ipairs(__supers) do
        if _iskindof(super, name) then
            return true
        end
    end
    return false
end

---
--- is kind of class
--- @param obj table|userdata object
--- @param classname string class name
function iskindof(obj, classname)
    local t = type(obj)
    if t ~= "table" and t ~= "userdata" then
        return false
    end

    local mt
    if t == "userdata" then
        if skynet_oo.iskindof(obj, classname) then
            return true
        end
        mt = getmetatable(skynet_oo.getpeer(obj))
    else
        mt = getmetatable(obj)
    end
    if mt then
        return _iskindof(mt, classname)
    end
    return false
end

