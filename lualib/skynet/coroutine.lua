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
    local thread = coroutine.create(f)
    skynet_coroutines[thread] = true -- mark thread as a skynet coroutine
    return thread
end

-- skynet_co.resume
do
    -- skynet use profile.resume_co/yield_co instead of coroutine.resume/yield
    local profile = require "skynet.profile"

    local profile_resume = profile.resume_co
    local profile_yield = profile.yield_co

    local function unlock(thread, ...)
        skynet_coroutines[thread] = true
        return ...
    end

    local function skynet_yielding(thread, from, ...)
        skynet_coroutines[thread] = false
        return unlock(thread, profile_resume(thread, from, profile_yield(from, ...)))
    end

    local function resume(thread, from, ok, ...)
        if not ok then
            return ok, ...
        elseif coroutine_status(thread) == "dead" then
            -- the main function exit
            skynet_coroutines[thread] = nil
            return true, ...
        elseif (...) == "USER" then
            return true, select(2, ...)
        else
            -- blocked in skynet framework, so raise the yielding message
            return resume(thread, from, skynet_yielding(thread, from, ...))
        end
    end

    -- record the root of coroutine caller (It should be a skynet thread)
    local coroutine_caller = setmetatable({}, { __mode = "kv" })

    function skynet_co.resume(thread, ...)
        local co_status = skynet_coroutines[thread]
        if not co_status then
            if co_status == false then
                -- is running
                return false, "cannot resume a skynet coroutine suspend by skynet framework"
            end
            if coroutine_status(thread) == "dead" then
                -- always return false, "cannot resume dead coroutine"
                return coroutine_resume(thread, ...)
            else
                return false, "cannot resume none skynet coroutine"
            end
        end
        local from = coroutine_running()
        local caller = coroutine_caller[from] or from
        coroutine_caller[thread] = caller
        return resume(thread, caller, coroutine_resume(thread, ...))
    end

    function skynet_co.thread(thread)
        thread = thread or coroutine_running()
        if skynet_coroutines[thread] ~= nil then
            return coroutine_caller[thread], false
        else
            return thread, true
        end
    end

end

--
function skynet_co.status(thread)
    local status = coroutine.status(thread)
    if status == "suspended" then
        if skynet_coroutines[thread] == false then
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
        local thread = skynet_co.create(function(...)
            return f(...)
        end)
        return function(...)
            return wrap_co(skynet_co.resume(thread, ...))
        end
    end

end

return skynet_co
