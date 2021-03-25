# 如何运行tests下的lua脚本

tests/ 下的 lua 脚本不能使用 lua 解释器直接运行。
需要先启动 skynet，用 skynet 加载它们。
如果打开了 console，这时应该可以在控制台输入一些字符。输入脚本的名字即可（不带 .lua）。
如果打开了 debug_console 可以用 telnet 连接上 127.0.0.1:8000 。然后试着输入 help ，学会怎样加载脚本。

