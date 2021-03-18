#pragma once

// forward declare
struct lua_State;

namespace skynet { namespace luaclib {

class skynet_netpack final
{
public:
    static int l_pop(lua_State* L);
    static int l_pack(lua_State* L);
    static int l_clear(lua_State* L);
    static int l_tostring(lua_State* L);
    static int l_filter(lua_State* L);
};

} }

