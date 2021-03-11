#------------------------------------------------------
# OS detect, can use them everywhere
#
# MACOSX    = MacOS X
# LINUX     = Linux
#------------------------------------------------------

if(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    SET(LINUX TRUE)
    SET(PLATFORM_FOLDER linux)
elseif(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    SET(APPLE TRUE)
    SET(MACOSX TRUE)
    SET(PLATFORM_FOLDER mac)
else()
    message(FATAL_ERROR "Unsupported platform, CMake will exit")
    return()
endif()

#------------------------------------------------------
# Build Mode
#------------------------------------------------------

# The default behavior of build
OPTION(DEBUG_MODE "Debug or Release?" ON)

# Debug is default value
if(NOT CMAKE_BUILD_TYPE)
    if(DEBUG_MODE)
        SET(CMAKE_BUILD_TYPE Debug)
    else()
        SET(CMAKE_BUILD_TYPE Release)
    endif()
endif()

if(CMAKE_GENERATOR)
    # generators that are capable of organizing into a hierarchy of folders
    SET_PROPERTY(GLOBAL PROPERTY USE_FOLDERS ON)

    # make configurations type keep same to cmake build type.
    SET(CMAKE_CONFIGURATION_TYPES "${CMAKE_BUILD_TYPE}" CACHE STRING "Reset the configurations to what we need" FORCE)
endif()

#------------------------------------------------------
# Custom properties
#------------------------------------------------------

# custom target property for dll collect
DEFINE_PROPERTY(TARGET
    PROPERTY CC_DEPEND_DLLS
    BRIEF_DOCS "depend dlls of a target"
    FULL_DOCS "use to save depend dlls of a target"
)
# custom target property for lua link
DEFINE_PROPERTY(TARGET
    PROPERTY CC_LUA_DEPEND
    BRIEF_DOCS "skynet lua depend libs"
    FULL_DOCS "use to save depend libs of skynet lua project"
)

#------------------------------------------------------
# Compiler check
#------------------------------------------------------

# Check CXX compiler
INCLUDE(CheckCXXCompilerFlag)
CHECK_CXX_COMPILER_FLAG("-std=c++17" COMPILER_SUPPORTS_CXX17)

# check c++ standard
SET(CMAKE_C_STANDARD 99)
SET(CMAKE_C_STANDARD_REQUIRED ON)
SET(CMAKE_CXX_STANDARD 17)
SET(CMAKE_CXX_STANDARD_REQUIRED ON)

if(COMPILER_SUPPORTS_CXX17)
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17")
else()
    MESSAGE(FATAL_ERROR "The compiler ${CMAKE_CXX_COMPILER} has no C++17 support. Please use a different C++ compiler.")
endif()

# Debug/Release support
SET(CMAKE_CXX_FLAGS_DEBUG   "$ENV{CXXFLAGS} -O0 -Wall -fPIC -g -ggdb") 
SET(CMAKE_CXX_FLAGS_RELEASE "$ENV{CXXFLAGS} -O3 -Wall -fPIC")

