--
-- You should use this module (skynet.coroutine) instead of origin lua coroutine in skynet framework
--

local coroutine = coroutine

-- origin lua coroutine module
local coroutine_resume = coroutine.resume
local coroutine_yield = coroutine.yield
local coroutine_status = coroutine.status
local coroutine_running = coroutine.running

local select = select

local skynet_co = {}

skynet_co.isyieldable = coroutine.isyieldable
skynet_co.running = coroutine.running
skynet_co.status = coroutine.status

-- skynet coroutines
local skynet_coroutines = setmetatable({}, { __mode = "kv" })

function skynet_co.create(f)
    local co = coroutine.create(f)
    skynet_coroutines[co] = true -- mark co as a skynet coroutine
    return co
end

-- skynet_co.resume
do
    -- skynet use profile.resume_co/yield_co instead of coroutine.resume/yield
    local profile = require "skynet.profile"

    local profile_resume = profile.resume_co
    local profile_yield = profile.yield_co

    local function unlock(co, ...)
        skynet_coroutines[co] = true
        return ...
    end

    local function skynet_yielding(co, from, ...)
        skynet_coroutines[co] = false
        return unlock(co, profile_resume(co, from, profile_yield(from, ...)))
    end

    local function resume(co, from, ok, ...)
        if not ok then
            return ok, ...
        elseif coroutine_status(co) == "dead" then
            -- the main function exit
            skynet_coroutines[co] = nil
            return true, ...
        elseif (...) == "USER" then
            return true, select(2, ...)
        else
            -- blocked in skynet framework, so raise the yielding message
            return resume(co, from, skynet_yielding(co, from, ...))
        end
    end

    -- record the root of coroutine caller (It should be a skynet thread)
    local coroutine_caller = setmetatable({}, { __mode = "kv" })

    function skynet_co.resume(co, ...)
        local co_status = skynet_coroutines[co]
        if not co_status then
            if co_status == false then
                -- is running
                return false, "cannot resume a skynet coroutine suspend by skynet framework"
            end
            if coroutine_status(co) == "dead" then
                -- always return false, "cannot resume dead coroutine"
                return coroutine_resume(co, ...)
            else
                return false, "cannot resume none skynet coroutine"
            end
        end
        local from = coroutine_running()
        local caller = coroutine_caller[from] or from
        coroutine_caller[co] = caller
        return resume(co, caller, coroutine_resume(co, ...))
    end

    function skynet_co.thread(co)
        co = co or coroutine_running()
        if skynet_coroutines[co] ~= nil then
            return coroutine_caller[co], false
        else
            return co, true
        end
    end

end

--
function skynet_co.status(co)
    local status = coroutine.status(co)
    if status == "suspended" then
        if skynet_coroutines[co] == false then
            return "blocked"
        else
            return "suspended"
        end
    else
        return status
    end
end

function skynet_co.yield(...)
    return coroutine_yield("USER", ...)
end

-- skynet_co.wrap
do
    local function wrap_co(ok, ...)
        if ok then
            return ...
        else
            error(...)
        end
    end

    function skynet_co.wrap(f)
        local co = skynet_co.create(function(...)
            return f(...)
        end)
        return function(...)
            return wrap_co(skynet_co.resume(co, ...))
        end
    end

end

return skynet_co
