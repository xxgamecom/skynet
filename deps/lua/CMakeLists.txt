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
    add_definitions(-DLUA_USE_MACOSX -DLUA_USE_READLINE)
elseif(LINUX)
    add_definitions(-DLUA_USE_LINUX -DLUA_USE_READLINE)
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


#lapi.o: lapi.c lprefix.h lua.h luaconf.h lapi.h llimits.h lstate.h lobject.h ltm.h lzio.h lmem.h ldebug.h ldo.h lfunc.h lgc.h lstring.h ltable.h lundump.h lvm.h
#lcode.o: lcode.c lprefix.h lua.h luaconf.h lcode.h llex.h lobject.h llimits.h lzio.h lmem.h lopcodes.h lparser.h ldebug.h lstate.h ltm.h ldo.h lgc.h lstring.h ltable.h lvm.h
#lctype.o: lctype.c lprefix.h lctype.h lua.h luaconf.h llimits.h
#ldebug.o: ldebug.c lprefix.h lua.h luaconf.h lapi.h llimits.h lstate.h lobject.h ltm.h lzio.h lmem.h lcode.h llex.h lopcodes.h lparser.h ldebug.h ldo.h lfunc.h lstring.h lgc.h ltable.h lvm.h
#ldo.o: ldo.c lprefix.h lua.h luaconf.h lapi.h llimits.h lstate.h lobject.h ltm.h lzio.h lmem.h ldebug.h ldo.h lfunc.h lgc.h lopcodes.h lparser.h lstring.h ltable.h lundump.h lvm.h
#ldump.o: ldump.c lprefix.h lua.h luaconf.h lobject.h llimits.h lstate.h ltm.h lzio.h lmem.h lundump.h
#lfunc.o: lfunc.c lprefix.h lua.h luaconf.h ldebug.h lstate.h lobject.h llimits.h ltm.h lzio.h lmem.h ldo.h lfunc.h lgc.h
#lgc.o: lgc.c lprefix.h lua.h luaconf.h ldebug.h lstate.h lobject.h llimits.h ltm.h lzio.h lmem.h ldo.h lfunc.h lgc.h lstring.h ltable.h
#llex.o: llex.c lprefix.h lua.h luaconf.h lctype.h llimits.h ldebug.h lstate.h lobject.h ltm.h lzio.h lmem.h ldo.h lgc.h llex.h lparser.h lstring.h ltable.h
#lmem.o: lmem.c lprefix.h lua.h luaconf.h ldebug.h lstate.h lobject.h llimits.h ltm.h lzio.h lmem.h ldo.h lgc.h
#lauxlib.o: lauxlib.c lprefix.h lua.h luaconf.h lauxlib.h
#lbaselib.o: lbaselib.c lprefix.h lua.h luaconf.h lauxlib.h lualib.h
#lcorolib.o: lcorolib.c lprefix.h lua.h luaconf.h lauxlib.h lualib.h
#ldblib.o: ldblib.c lprefix.h lua.h luaconf.h lauxlib.h lualib.h
#linit.o: linit.c lprefix.h lua.h luaconf.h lualib.h lauxlib.h
#liolib.o: liolib.c lprefix.h lua.h luaconf.h lauxlib.h lualib.h
#lmathlib.o: lmathlib.c lprefix.h lua.h luaconf.h lauxlib.h lualib.h
#loadlib.o: loadlib.c lprefix.h lua.h luaconf.h lauxlib.h lualib.h
#lobject.o: lobject.c lprefix.h lua.h luaconf.h lctype.h llimits.h ldebug.h lstate.h lobject.h ltm.h lzio.h lmem.h ldo.h lstring.h lgc.h lvm.h
#lopcodes.o: lopcodes.c lprefix.h lopcodes.h llimits.h lua.h luaconf.h
#loslib.o: loslib.c lprefix.h lua.h luaconf.h lauxlib.h lualib.h
#lparser.o: lparser.c lprefix.h lua.h luaconf.h lcode.h llex.h lobject.h llimits.h lzio.h lmem.h lopcodes.h lparser.h ldebug.h lstate.h ltm.h ldo.h lfunc.h lstring.h lgc.h ltable.h
#lstate.o: lstate.c lprefix.h lua.h luaconf.h lapi.h llimits.h lstate.h lobject.h ltm.h lzio.h lmem.h ldebug.h ldo.h lfunc.h lgc.h llex.h lstring.h ltable.h
#lstring.o: lstring.c lprefix.h lua.h luaconf.h ldebug.h lstate.h lobject.h llimits.h ltm.h lzio.h lmem.h ldo.h lstring.h lgc.h
#lstrlib.o: lstrlib.c lprefix.h lua.h luaconf.h lauxlib.h lualib.h
#ltable.o: ltable.c lprefix.h lua.h luaconf.h ldebug.h lstate.h lobject.h llimits.h ltm.h lzio.h lmem.h ldo.h lgc.h lstring.h ltable.h lvm.h
#ltablib.o: ltablib.c lprefix.h lua.h luaconf.h lauxlib.h lualib.h
#ltm.o: ltm.c lprefix.h lua.h luaconf.h ldebug.h lstate.h lobject.h llimits.h ltm.h lzio.h lmem.h ldo.h lgc.h lstring.h ltable.h lvm.h
#lua.o: lua.c lprefix.h lua.h luaconf.h lauxlib.h lualib.h
#luac.o: luac.c lprefix.h lua.h luaconf.h lauxlib.h ldebug.h lstate.h lobject.h llimits.h ltm.h lzio.h lmem.h lopcodes.h lopnames.h lundump.h
#lundump.o: lundump.c lprefix.h lua.h luaconf.h ldebug.h lstate.h lobject.h llimits.h ltm.h lzio.h lmem.h ldo.h lfunc.h lstring.h lgc.h lundump.h
#lutf8lib.o: lutf8lib.c lprefix.h lua.h luaconf.h lauxlib.h lualib.h
#lvm.o: lvm.c lprefix.h lua.h luaconf.h ldebug.h lstate.h lobject.h llimits.h ltm.h lzio.h lmem.h ldo.h lfunc.h lgc.h lopcodes.h lstring.h ltable.h lvm.h ljumptab.h
#lzio.o: lzio.c lprefix.h lua.h luaconf.h llimits.h lmem.h lstate.h lobject.h ltm.h lzio.h