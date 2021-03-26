--local currentFilePath = (...):gsub("%.init$", "")
--
--local plural = require(currentFilePath .. '.plural') -- 复数处理
--local interpolate = require(currentFilePath .. '.interpolate') -- 插值处理
--local variants = require(currentFilePath .. '.variants') --
--local version = require(currentFilePath .. '.version')

local plural = require('plural') -- 复数处理
local interpolate = require('interpolate') -- 插值处理
local variants = require('variants') --
local version = require('version')

local pairs = pairs
local tostring = tostring

local M = {
	plural = plural,
	interpolate = interpolate,
	variants = variants,
	version = version,
	_VERSION = version
}

-- ------------------------------------------------------
-- private stuff
-- ------------------------------------------------------

local store --
local locale -- 本地语言
local pluralizeFunction -- 复数处理函数
local defaultLocale = 'en' -- 默认语言
local fallbackLocale = defaultLocale -- 失败时的默认语言

-- @param str
local function dotSplit(str)
	local fields, length = {}, 0
	str:gsub("[^%.]+", function(c)
		length = length + 1
		fields[length] = c
	end)
	return fields, length
end

local function isPluralTable(t)
	return type(t) == 'table' and type(t.other) == 'string'
end

local function isPresent(str)
	return type(str) == 'string' and #str > 0
end

local function assertPresent(functionName, paramName, value)
	if isPresent(value) then
		return
	end

	local msg = "i18n.%s requires a non-empty string on its %s. Got %s (a %s value)."
	error(msg:format(functionName, paramName, tostring(value), type(value)))
end

local function assertPresentOrPlural(functionName, paramName, value)
	if isPresent(value) or isPluralTable(value) then
		return
	end

	local msg = "i18n.%s requires a non-empty string or plural-form table on its %s. Got %s (a %s value)."
	error(msg:format(functionName, paramName, tostring(value), type(value)))
end

local function assertPresentOrTable(functionName, paramName, value)
	if isPresent(value) or type(value) == 'table' then
		return
	end

	local msg = "i18n.%s requires a non-empty string or table on its %s. Got %s (a %s value)."
	error(msg:format(functionName, paramName, tostring(value), type(value)))
end

local function assertFunctionOrNil(functionName, paramName, value)
	if value == nil or type(value) == 'function' then
		return
	end

	local msg = "i18n.%s requires a function (or nil) on param %s. Got %s (a %s value)."
	error(msg:format(functionName, paramName, tostring(value), type(value)))
end

local function defaultPluralizeFunction(count)
	return plural.get(variants.root(M.getLocale()), count)
end

local function pluralize(t, data)
	assertPresentOrPlural('interpolatePluralTable', 't', t)
	data = data or {}
	local count = data.count or 1
	local plural_form = pluralizeFunction(count)
	return t[plural_form]
end

local function treatNode(node, data)
	if type(node) == 'string' then
		return interpolate(node, data)
	elseif isPluralTable(node) then
		return interpolate(pluralize(node, data), data)
	end
	return node
end

-- 递归加载
local function recursiveLoad(currentContext, data)
	local composedKey
	for k, v in pairs(data) do
		composedKey = (currentContext and (currentContext .. '.') or "") .. tostring(k)
		assertPresent('load', composedKey, tostring(k))
		assertPresentOrTable('load', composedKey, v)
		if type(v) == 'string' then
			M.set(composedKey, v)
		else
			recursiveLoad(composedKey, v)
		end
	end
end

local function localizedTranslate(key, loc, data)
	local path, length = dotSplit(loc .. "." .. key)
	local node = store

	for i = 1, length do
		node = node[path[i]]
		if not node then
			return nil
		end
	end

	return treatNode(node, data)
end

-- ------------------------------------------------------
-- public interface
-- ------------------------------------------------------

-- @param key 格式: en.key
-- @param value
function M.set(key, value)
	-- key为number时, 转换为string处理
	if type(key) == 'number' then
		key = tostring(key)
	end

	assertPresent('set', 'key', key)
	assertPresentOrPlural('set', 'value', value)

	local path, length = dotSplit(key)
	local node = store

	for i = 1, length - 1 do
		key = path[i]
		node[key] = node[key] or {}
		node = node[key]
	end

	local lastKey = path[length]
	node[lastKey] = value
end

--
function M.setLocale(newLocale, newPluralizeFunction)
	assertPresent('setLocale', 'newLocale', newLocale)
	assertFunctionOrNil('setLocale', 'newPluralizeFunction', newPluralizeFunction)
	locale = newLocale
	pluralizeFunction = newPluralizeFunction or defaultPluralizeFunction
end

--
function M.getLocale()
	return locale
end

--
function M.setFallbackLocale(newFallbackLocale)
	assertPresent('setFallbackLocale', 'newFallbackLocale', newFallbackLocale)
	fallbackLocale = newFallbackLocale
end

--
function M.getFallbackLocale()
	return fallbackLocale
end

--
function M.load(data)
	recursiveLoad(nil, data)
end

-- 从文件加载翻译数据
function M.loadFile(path)
	local chunk = assert(loadfile(path))
	local data = chunk()
	M.load(data)
end

-- 翻译
-- @param key
-- @param data {locale = 'xxx'}
function M.translate(key, data)
	-- key为number时, 转换为string处理
	if type(key) == 'number' then
		key = tostring(key)
	end

	--
	assertPresent('translate', 'key', key)

	data = data or {}
	local usedLocale = data.locale or locale

	-- 容错
	local fallbacks = variants.fallbacks(usedLocale, fallbackLocale)
	for i = 1, #fallbacks do
		local value = localizedTranslate(key, fallbacks[i], data)
		if value then
			return value
		end
	end

	return data.default
end

--
function M.reset()
	store = {}
	plural.reset()
	M.setLocale(defaultLocale)
	M.setFallbackLocale(defaultLocale)
end

M.reset()

return M
