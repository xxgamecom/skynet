# lua core source file
SET(LUA_CORE_SRC
    lapi.c
    lcode.c
    lctype.c
    ldebug.c
    ldo.c
    ldump.c
    lfunc.c
    lgc.c
    llex.c
    lmem.c
    lobject.c
    lopcodes.c
    lparser.c
    lstate.c
    lstring.c
    ltable.c
    ltm.c
    lundump.c
    lvm.c
    lzio.c
)

# lua lib source file
SET(LUA_LIB_SRC
    lauxlib.c
    lbaselib.c
    lcorolib.c
    ldblib.c
    liolib.c
    lmathlib.c
    loslib.c
    lstrlib.c
    ltablib.c
    lutf8lib.c
    loadlib.c
    linit.c
)

# lua source file
SET(LUA_SRC
    lua.c
)

# luac source file
SET(LUAC_SRC
    luac.c
)

# target liblua.a
ADD_LIBRARY(liblua STATIC 
    ${LUA_CORE_SRC}
    ${LUA_LIB_SRC}
)

SET_TARGET_PROPERTIES(liblua
    PROPERTIES
    OUTPUT_NAME "lua"
)

# target lua
ADD_EXECUTABLE(lua ${LUA_SRC})
TARGET_LINK_LIBRARIES(lua dl readline liblua)

# target luac
ADD_EXECUTABLE(luac ${LUAC_SRC})
TARGET_LINK_LIBRARIES(luac dl readline liblua)
