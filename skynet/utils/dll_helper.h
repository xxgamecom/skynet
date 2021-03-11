#pragma once

#include <dlfcn.h>
#include <string>

namespace skynet {

// dll utils
class dll_helper final
{
public:
    // load dll
    static void* load(std::string filename);

    // get api
    template<typename SYMBOL>
    static SYMBOL get_symbol(void* handle, std::string symbol);

    // 
    static std::string error();
};

}

#include "dll_helper.inl"

