namespace skynet {

inline bool service_module::is_dll_loaded()
{
    return handle != nullptr;
}

inline bool service_module::is_api_loaded()
{
    return init_func != nullptr;
}

inline bool module_manager::init(const std::string search_path)
{
    count_ = 0;
    search_path_ = search_path;

    return true;
}

inline void* module_manager::instance_create(service_module* mod)
{
    if (mod->create_func != nullptr)
        return mod->create_func();
    else
        return (void*)(intptr_t)(~0);
}

inline int module_manager::instance_init(service_module* mod, void* inst, skynet_context* ctx, const char* param)
{
    assert(mod->init_func != nullptr);
    return mod->init_func(inst, ctx, param);
}

inline void module_manager::instance_release(service_module* mod, void *inst)
{
    if (mod->release_func != nullptr)
        mod->release_func(inst);
}

inline void module_manager::instance_signal(service_module* mod, void* inst, int signal)
{
    if (mod->signal_func != nullptr)
        mod->signal_func(inst, signal);
}

inline service_module* module_manager::_find_loaded_mod(const std::string& mod_name)
{
    return std::find_if(service_mods_.begin(), service_mods_.end(), [&](const service_module& m) {
        return m.name == mod_name;
    });
}

inline bool module_manager::_load_mod_api(service_module& mod)
{
    mod.create_func  = _get_symbol<mod_create_proc>(mod, mod.name + "_create");
    mod.init_func    = _get_symbol<mod_init_proc>(mod, mod.name + "_init");
    mod.release_func = _get_symbol<mod_release_proc>(mod, mod.name + "_release");
    mod.signal_func  = _get_symbol<mod_signal_proc>(mod, mod.name + "_signal");

    return mod.init_func == nullptr;
}

template<typename SYMBOL>
inline SYMBOL module_manager::_get_symbol(service_module& mod, std::string symbol)
{
    if (!mod.is_dll_loaded())
        return nullptr;

    return (SYMBOL)::dlsym(mod.handle, symbol.c_str());
}

}
