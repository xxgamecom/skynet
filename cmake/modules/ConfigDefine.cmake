#------------------------------------------------------
# OS detect, can use them everywhere
#
# MACOSX    = MacOS X
# LINUX     = Linux
#------------------------------------------------------

if(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    set(LINUX TRUE)
    set(PLATFORM_FOLDER linux)
elseif(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    set(APPLE TRUE)
    set(MACOSX TRUE)
    set(PLATFORM_FOLDER mac)
else()
    message(FATAL_ERROR "Unsupported platform, CMake will exit")
    return()
endif()

#------------------------------------------------------
# Build Mode
#------------------------------------------------------

# The default behavior of build
option(DEBUG_MODE "Debug or Release?" ON)

# Debug is default value
if(NOT CMAKE_BUILD_TYPE)
    if(DEBUG_MODE)
        set(CMAKE_BUILD_TYPE Debug)
    else()
        set(CMAKE_BUILD_TYPE Release)
    endif()
endif()

if(CMAKE_GENERATOR)
    # generators that are capable of organizing into a hierarchy of folders
    set_property(GLOBAL PROPERTY USE_FOLDERS ON)

    # make configurations type keep same to cmake build type.
    set(CMAKE_CONFIGURATION_TYPES "${CMAKE_BUILD_TYPE}" CACHE STRING "Reset the configurations to what we need" FORCE)
endif()

#------------------------------------------------------
# Compiler check
#------------------------------------------------------

# Check CXX compiler
include(CheckCXXCompilerFlag)
check_cxx_compiler_flag("-std=c++17" COMPILER_SUPPORTS_CXX17)

# check c++ standard
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(COMPILER_SUPPORTS_CXX17)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17")
else()
    message(FATAL_ERROR "The compiler ${CMAKE_CXX_COMPILER} has no C++17 support. Please use a different C++ compiler.")
endif()



