--
-- skynet module two-step initialize. When you require a skynet module:
-- 1. Run module main function as official lua module behavior.
-- 2. Run the functions register by skynet.init() during the step 1, unless calling `require` in main thread.
--
-- If you call `require` in main thread ( service main function ), the functions
-- registered by skynet.init() do not execute immediately, they will be executed
-- by skynet.start() before start function.
--

local M = {}

-- must initialize in main thread
local main_thread, is_main = coroutine.running()
assert(is_main, "skynet.require must initialize in main thread")

--
local context = {
    [main_thread] = {},
}

do
    local require = _G.require      -- backup official require
    local loaded = package.loaded   -- has loaded module
    local loading = {}              -- module loading queue

    ---
    --- require skynet module
    ---@param name string
    function M.require(name)
        -- has loaded
        local m = loaded[name]
        if m ~= nil then
            return m
        end

        -- in main thread, use official load it immediately
        local current_thread, main = coroutine.running()
        if main then
            return require(name)
        end

        -- not in main thread, try search in package.path
        local filename = package.searchpath(name, package.path)
        -- the module not in package.path, use official load it immediately
        if not filename then
            return require(name)
        end

        -- load the module file, if failed use official load it immediately
        local modfunc = loadfile(filename)
        if not modfunc then
            return require(name)
        end

        -- check loading queue
        local loading_queue = loading[name]
        if loading_queue then
            -- Module is in the init process (require the same mod at the same time in different coroutines), waiting.
            local skynet = require("skynet")
            loading_queue[#loading_queue + 1] = current_thread
            skynet.wait(current_thread)

            --
            local m = loaded[name]
            if m == nil then
                error(string.format("require %s failed", name))
            end
            return m
        end

        --
        loading_queue = {}
        loading[name] = loading_queue

        local old_init_list = context[current_thread]
        local init_list = {}
        context[current_thread] = init_list

        -- We should call modfunc in lua, because modfunc may yield by calling M.require recursive.
        local function execute_module()
            local m = modfunc(name, filename)

            for _, func in ipairs(init_list) do
                func()
            end

            if m == nil then
                m = true
            end

            loaded[name] = m
        end

        local ok, err = xpcall(execute_module, debug.traceback)

        context[current_thread] = old_init_list

        local waiting = #loading_queue
        if waiting > 0 then
            local skynet = require("skynet")
            for i = 1, waiting do
                skynet.wakeup(loading_queue[i])
            end
        end
        loading[name] = nil

        if ok then
            return loaded[name]
        else
            error(err)
        end
    end
end

---
---
function M.init_all()
    for _, func in ipairs(context[main_thread]) do
        func()
    end
    context[main_thread] = nil
end

---
--- register the initialize function
---@param func function the initialize function
function M.init(func)
    assert(type(func) == "function")
    local current_thread = coroutine.running()
    table.insert(context[current_thread], func)
end

return M
