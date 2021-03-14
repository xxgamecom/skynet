namespace skynet {

inline bool cservice_mod_manager::init(const std::string search_path)
{
    count_ = 0;
    search_path_ = search_path;

    return true;
}

inline cservice_mod* cservice_mod_manager::_find_loaded_mod(const std::string& mod_name)
{
    return std::find_if(cservice_mods_.begin(), cservice_mods_.end(), [&](const cservice_mod& m) {
        return m.name == mod_name;
    });
}

inline bool cservice_mod_manager::_try_load_mod_api(cservice_mod& mod)
{
    assert(mod.dll_handle != nullptr);
    
    mod.create_func  = dll_helper::get_symbol<mod_create_proc>(mod.dll_handle, mod.name + "_create");
    mod.init_func    = dll_helper::get_symbol<mod_init_proc>(mod.dll_handle, mod.name + "_init");
    mod.release_func = dll_helper::get_symbol<mod_release_proc>(mod.dll_handle, mod.name + "_release");
    mod.signal_func  = dll_helper::get_symbol<mod_signal_proc>(mod.dll_handle, mod.name + "_signal");

    return mod.init_func != nullptr;
}

}
