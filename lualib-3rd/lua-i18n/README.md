i18n.lua
========

[![Build Status](https://travis-ci.org/kikito/i18n.lua.png?branch=master)](https://travis-ci.org/kikito/i18n.lua)

A very complete i18n lib for Lua

描述
====

``` lua
i18n = require 'i18n'

-- 加载翻译数据
i18n.set('en.welcome', 'welcome to this program')
i18n.load({
  en = {
    good_bye = "good-bye!",
    age_msg = "your age is %{age}.",
    phone_msg = {
      one = "you have one new message.",
      other = "you have %{count} new messages."
    }
  }
})
i18n.loadFile('path/to/your/project/i18n/de.lua') -- load German language file
i18n.loadFile('path/to/your/project/i18n/fr.lua') -- load French language file
…         -- section 'using language files' below describes structure of files

-- 设置翻译环境
i18n.setLocale('en') -- 默认本地语言环境为英语

-- 获取译文
i18n.translate('welcome') -- Welcome to this program
i18n.translate('age_msg', {age = 18}) -- Your age is 18.
i18n.translate('phone_msg', {count = 1}) -- You have one new message.
i18n.translate('phone_msg', {count = 2}) -- You have 2 new messages.
i18n.translate('good_bye') -- Good-bye!

```

插值处理
=======

添加变量的3种方式：

``` lua
-- 最常用的一种
i18n.set('variables', 'Interpolating variables: %{name} %{age}')
i18n.translate('variables', {name='john', 'age'=10}) -- Interpolating variables: john 10

i18n.set('lua', 'Traditional Lua way: %d %s')
i18n.translate('lua', {1, 'message'}) -- Traditional Lua way: 1 message

i18n.set('combined', 'Combined: %<name>.q %<age>.d %<age>.o')
i18n.translate('combined', {name='john', 'age'=10}) -- Combined: john 10 12k
```

复数处理
=======

This lib implements the [unicode.org plural rules](http://cldr.unicode.org/index/cldr-spec/plural-rules). Just set the locale you want to use and it will deduce the appropiate pluralization rules:

``` lua
i18n = require 'i18n'

i18n.load({
  en = {
    msg = {
      one   = "one message",
      other = "%{count} messages"
    }
  },
  ru = {
    msg = {
      one   = "1 сообщение",
      few   = "%{count} сообщения",
      many  = "%{count} сообщений",
      other = "%{count} сообщения"
    }
  }
})

i18n.translate('msg', {count = 1}) -- one message
i18n.setLocale('ru')
i18n.translate('msg', {count = 5}) -- 5 сообщений
```

The appropiate rule is chosen by finding the 'root' of the locale used: for example if the current locale is 'fr-CA', the 'fr' rules will be applied.

If the provided functions are not enough (i.e. invented languages) it's possible to specify a custom pluralization function in the second parameter of setLocale. This function must return 'one', 'few', 'other', etc given a number.

容错机制
=======

当一个值没有找到时，该库有以下几种容错机制：

* 首先, it will look in the current locale's parents. For example, if the locale was set to 'en-US' and the key 'msg' was not found there, it will be looked over in 'en'.
* Second, if the value is not found in the locale ancestry, a 'fallback locale' (by default: 'en') can be used. If the fallback locale has any parents, they will be looked over too.
* Third, if all the locales have failed, but there is a param called 'default' on the provided data, it will be used.
* Otherwise the translation will return nil.

The parents of a locale are found by splitting the locale by its hyphens. Other separation characters (spaces, underscores, etc) are not supported.

使用语言文件
==========

可以将各种语言放到不同的文件，通过 'i18n.loadFile' 指令加载:

``` lua
…
i18n.loadFile('path/to/your/project/i18n/de.lua') -- German translation
i18n.loadFile('path/to/your/project/i18n/en.lua') -- English translation
i18n.loadFile('path/to/your/project/i18n/fr.lua') -- French translation
…
```

德语文件 'de.lua'：

``` lua
return {
  de = {
    good_bye = "Auf Wiedersehen!",
    age_msg = "Ihr Alter beträgt %{age}.",
    phone_msg = {
      one = "Sie haben eine neue Nachricht.",
      other = "Sie haben %{count} neue Nachrichten."
    }
  }
}
```

你也可以将所有语言放到一个文件中 (例如: 'translations.lua')：

``` lua
return {
  de = {
    good_bye = "Auf Wiedersehen!",
    age_msg = "Ihr Alter beträgt %{age}.",
    phone_msg = {
      one = "Sie haben eine neue Nachricht.",
      other = "Sie haben %{count} neue Nachrichten."
    }
  },
  fr = {
    good_bye = "Au revoir !",
    age_msg = "Vous avez %{age} ans.",
    phone_msg = {
      one = "Vous avez une noveau message.",
      other = "Vous avez %{count} noveaux messages."
    }
  },
  …
}
```

例子
===

lua ./example/example1.lua

Specs
=====
This project uses [busted](https://github.com/Olivine-Labs/busted) for its specs. If you want to run the specs, you will have to install it first. Then just execute the following from the root inspect folder:

    busted
