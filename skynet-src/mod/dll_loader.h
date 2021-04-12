#pragma once

#include <string>
#include <dlfcn.h>

namespace skynet {

typedef void* dll_handle;

class dll_loader final
{
private:
    dll_handle handle_ = nullptr;                   // dll mod handle

public:
    dll_loader() = default;
    ~dll_loader();

    // no copy & assign
public:
    dll_loader(const dll_loader&) = delete;
    dll_loader& operator=(const dll_loader&) = delete;

public:
    bool load(std::string filename);
    void unload();

    template<typename SYMBOL>
    SYMBOL get_symbol(const char* symbol);

    bool is_loaded();

    std::string error();
};

} 

#include "dll_loader.inl"
