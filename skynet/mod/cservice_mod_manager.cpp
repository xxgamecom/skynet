#include "cservice_mod_manager.h"

#include <iostream>
#include <algorithm>
#include <regex>

namespace skynet {

// singleton
cservice_mod_manager* cservice_mod_manager::instance_ = nullptr;

cservice_mod_manager* cservice_mod_manager::instance()
{
    static std::once_flag once;
    std::call_once(once, [&]() { instance_ = new cservice_mod_manager; });
    
    return instance_;
}

bool cservice_mod_manager::add(cservice_mod* mod)
{
    // scope lock
    std::lock_guard<std::mutex> lock(mutex_);

    // already added, just return
    cservice_mod* exist_mod = _find_loaded_mod(mod->name);
    if (exist_mod != nullptr)
        return true;
    
    // reach limit
    if (count_ >= MAX_MODULE_NUM)
        return false;

    // add
    cservice_mods_[count_++] = *mod;
    return true;
}

cservice_mod* cservice_mod_manager::query(const std::string mod_name)
{
    // return exists mod (fast find, not in scope-lock area)
    cservice_mod* mod = _find_loaded_mod(mod_name);
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
    
    // try to load c service mod mod_name.so
    void* handle = _try_load_mod_dll(mod_name);
    if (handle == nullptr)
        return nullptr;

    int idx = count_;
    cservice_mods_[idx].name = mod_name;
    cservice_mods_[idx].dll_handle = handle;

    // try to load c service mod APIs
    if (!_try_load_mod_api(cservice_mods_[idx]))
    {
        cservice_mods_[idx].name = mod_name;
        ++count_;
        mod = &cservice_mods_[idx];
    }

    return mod;
}

void* cservice_mod_manager::_try_load_mod_dll(const std::string& mod_name)
{
    // notice: search path divide by ';' wildcard: '?'

    // split by ';'
    std::regex re {';'};
    std::vector<std::string> paths {
        std::sregex_token_iterator(search_path_.begin(), search_path_.end(), re, -1),
        std::sregex_token_iterator()
    };

    // load mod_name.so
    void* dll_handle = nullptr;
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
        dll_handle = dll_helper::load(mod_path);
        if (dll_handle != nullptr)
            break;
    }

    if (dll_handle == nullptr)
    {
        std::cerr << "try open c service mod(" << mod_name << ") failed : " << dll_helper::error() << std::endl;
    }

    return dll_handle;
}

}

