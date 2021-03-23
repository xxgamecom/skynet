# lua core source file
set(LUA_CORE_SRC
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
set(LUA_LIB_SRC
    lauxlib.c
    lbaselib.c
    lbitlib.c
    lcorolib.c
    ldblib.c
    liolib.c
    lmathlib.c
    loadlib.c
    loslib.c
    lstrlib.c
    ltablib.c
    lutf8lib.c
    linit.c
)

# lua source file
set(LUA_SRC lua.c)

# luac source file
set(LUAC_SRC luac.c)

#
if (MACOSX)
    add_definitions(-DLUA_USE_MACOSX)
elseif(LINUX)
    add_definitions(-DLUA_USE_LINUX -Wl,-E)
endif()

# target liblua.a
add_library(liblua STATIC
    ${LUA_CORE_SRC}
    ${LUA_LIB_SRC}
)
set_target_properties(liblua
    PROPERTIES
    OUTPUT_NAME "lua"
)

# target lua
add_executable(lua ${LUA_SRC})
target_link_libraries(lua m dl readline liblua)

# target luac
add_executable(luac ${LUAC_SRC})
target_link_libraries(luac m dl readline liblua)

