#pragma once

struct lua_State;
struct lua_Debug;

namespace skynet { namespace service {

//
void signal_hook(lua_State* L, lua_Debug* ar);
//
int open_skynet_profile(lua_State* L);


} }
