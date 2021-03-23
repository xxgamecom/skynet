namespace skynet {

// 加载动态库
inline bool dll_loader::load(std::string filename)
{
    // already loaded
    if (is_loaded())
        return true;

    if (!filename.empty())
    {
        handle_ = ::dlopen(filename.c_str(), RTLD_NOW | RTLD_GLOBAL);
    }

    return handle_ != nullptr;
}

// 卸载动态库
inline void dll_loader::unload()
{
    if (handle_ != nullptr)
    {
        ::dlclose(handle_);
        handle_ = nullptr;
    }
}

// 获取符号
template<typename SYMBOL>
inline SYMBOL dll_loader::get_symbol(const char* symbol)
{
    if (!is_loaded())
        return nullptr;

    return (SYMBOL)::dlsym(handle_, symbol);
}

// 是否已经加载动态库
inline bool dll_loader::is_loaded()
{
    return handle_ != nullptr;
}

inline std::string dll_loader::error()
{
    return ::dlerror();
}

}
