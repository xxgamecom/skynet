#pragma once

extern "C" {
#include <lua.h>
}

int luaseri_pack(lua_State* L);
int luaseri_unpack(lua_State* L);

