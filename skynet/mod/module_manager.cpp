#include "module_manager.h"

#include <iostream>
#include <algorithm>
#include <regex>

namespace skynet {

// singleton
module_manager* module_manager::instance_ = nullptr;

module_manager* module_manager::instance()
{
    static std::once_flag once;
    std::call_once(once, [&]() { instance_ = new module_manager; });
    
    return instance_;
}

bool module_manager::add(service_module* mod)
{
    // scope lock
    std::lock_guard<std::mutex> lock(mutex_);

    // already added, just return
    service_module* exist_mod = _find_loaded_mod(mod->name);
    if (exist_mod != nullptr)
        return true;
    
    // reach limit
    if (count_ >= MAX_MODULE_NUM)
        return false;

    // add
    service_mods_[count_++] = *mod;
    return true;
}

service_module* module_manager::query(const std::string mod_name)
{
    // return exists mod (fast find, not in scope-lock area)
    service_module* mod = _find_loaded_mod(mod_name);
    if (mod != nullptr)
        return mod;

    // find use scope-lock
    std::lock_guard<std::mutex> lock(mutex_);

    // reach limit
    if (count_ >= MAX_MODULE_NUM)
        return nullptr;

    // find exist (double check)
    mod = _find_loaded_mod(mod_name);
    if (mod != nullptr)
        return mod;
    
    // try to load mod_name.so
    dll_handle handle = _try_load_mod(mod_name);
    if (handle == nullptr)
        return nullptr;

    int index = count_;
    service_mods_[index].name = mod_name;
    service_mods_[index].handle = handle;

    // load c service mod APIs
    if (!_load_mod_api(service_mods_[index]))
    {
        service_mods_[index].name = mod_name;
        ++count_;
        mod = &service_mods_[index];
    }

    return mod;
}

dll_handle module_manager::_try_load_mod(const std::string& mod_name)
{
    // notice: search path divide by ';' wildcard: '?'

    // split by ';'
    std::regex re {';'};
    std::vector<std::string> paths {
        std::sregex_token_iterator(search_path_.begin(), search_path_.end(), re, -1),
        std::sregex_token_iterator()
    };

    // load mod_name.so
    dll_handle handle = nullptr;
    for (auto& path : paths)
    {
        // find wildcard '?'
        int pos = path.find('?');
        if (pos == path.npos)
        {
            std::cerr << "Invalid C service path" << std::endl;
            ::exit(1);
        }

        // mod file path
        std::string mod_path = path.substr(0, pos) + mod_name;

        // 
        handle = ::dlopen(mod_path.c_str(), RTLD_NOW | RTLD_GLOBAL);
        if (handle != nullptr)
            break;
    }

    if (handle == nullptr)
    {
        std::cerr << "try open " << mod_name << " failed : " << ::dlerror() << std::endl;
    }

    return handle;
}

}

