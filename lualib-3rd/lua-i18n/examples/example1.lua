local i18n = require 'i18n'

i18n.loadFile('examples/en-us.lua')
i18n.loadFile('examples/zh-cn.lua')
i18n.loadFile('examples/zh-tw.lua')

-- 中文简体
print(i18n.translate('hello', { locale = 'zh-cn' }))
print(i18n.translate('balance', { locale = 'zh-cn', value = 18 }))
print(i18n.translate(100, { locale = 'zh-cn' }))
print('')

-- 中文繁体
print(i18n.translate('hello', { locale = 'zh-tw' }))
print(i18n.translate('balance', { locale = 'zh-tw', value = 18 }))
print(i18n.translate(100, { locale = 'zh-tw' }))
print('')

-- 英文
print(i18n.translate('hello', { locale = 'en-us' }))
print(i18n.translate('balance', { locale = 'en-us', value = 18 }))
print(i18n.translate(100, { locale = 'en-us' }))
print('')
