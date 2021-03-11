namespace skynet {

// load dll
inline void* dll_helper::load(std::string filename)
{
    if (filename.empty())
        return nullptr;

    return ::dlopen(filename.c_str(), RTLD_NOW | RTLD_GLOBAL);
}

template<typename SYMBOL>
inline SYMBOL dll_helper::get_symbol(void* dll_handle, std::string symbol)
{
    if (dll_handle == nullptr)
        return nullptr;

    return (SYMBOL)::dlsym(dll_handle, symbol.c_str());
}

inline std::string dll_helper::error()
{
    return ::dlerror();
}

}

