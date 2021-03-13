#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <lua.h>

#ifdef __cplusplus
}
#endif

int luaseri_pack(lua_State* L);
int luaseri_unpack(lua_State* L);


