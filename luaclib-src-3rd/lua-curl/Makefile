
LUA_P :=../../skynet/
LUA_STATICLIB := $(LUA_P)3rd/lua/liblua.a
LUA_LIB ?= $(LUA_STATICLIB)
LUA_INC ?= $(LUA_P)3rd/lua

all:
	gcc -I$(LUA_INC) $(LUA_LIB) -Wall -O2 -shared -o luacurl.so luacurl.c -lssl -lcrypto -fPIC  -lcurl

clean:
	rm -f luacurl.so
