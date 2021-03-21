#include "mod_manager.h"

#include <iostream>
#include <algorithm>
#include <regex>
#include <utility>

namespace skynet {

//
// singleton
//

mod_manager* mod_manager::instance_ = nullptr;

mod_manager* mod_manager::instance()
{
    static std::once_flag once;
    std::call_once(once, [&]() { instance_ = new mod_manager; });
    
    return instance_;
}

//
// mod_manager
//

bool mod_manager::init(std::string search_path)
{
    search_path_ = std::move(search_path);
    return true;
}

void mod_manager::fini()
{
    std::lock_guard<std::mutex> lock(mutex_);

    service_mod_info* mod_info_ptr;
    for (auto& itr : service_mod_map_)
    {
        mod_info_ptr = itr.second;
        mod_info_ptr->dll_loader_.unload();
        delete mod_info_ptr;
    }
    service_mod_map_.clear();
}

service_mod_info* mod_manager::query(std::string mod_name)
{
    std::lock_guard<std::mutex> lock(mutex_);

    service_mod_info* mod_info_ptr = nullptr;
    auto itr_find = service_mod_map_.find(mod_name);
    if (itr_find != service_mod_map_.end())
    {
        mod_info_ptr = itr_find->second;
    }

    return mod_info_ptr;
}

// load c service mod
service_mod_info* mod_manager::load(std::string mod_name)
{
    std::lock_guard<std::mutex> lock(mutex_);

    // load mod dll
    auto mod_info_ptr = _try_load_dll(mod_name);
    if (mod_info_ptr == nullptr)
    {
        return nullptr;
    }

    // get mod api
    mod_info_ptr->create_func_  = mod_info_ptr->dll_loader_.get_symbol<create_cservice_proc>(CREATE_CSERVICE_PROC);
    mod_info_ptr->release_func_ = mod_info_ptr->dll_loader_.get_symbol<release_cservice_proc>(RELEASE_CSERVICE_PROC);
    if (mod_info_ptr->create_func_ == nullptr || mod_info_ptr->release_func_ == nullptr)
    {
        delete mod_info_ptr;
        return nullptr;
    }

    // store
    service_mod_map_[mod_name] = mod_info_ptr;

    return mod_info_ptr;
}

service_mod_info* mod_manager::_try_load_dll(const std::string& mod_name)
{
    auto mod_info_ptr = new service_mod_info;
    mod_info_ptr->name_ = mod_name;

    // notice: search path divide by ';' wildcard: '?'

    // split by ';'
    std::regex re {';'};
    std::vector<std::string> paths {
        std::sregex_token_iterator(search_path_.begin(), search_path_.end(), re, -1),
        std::sregex_token_iterator()
    };

    // try load mod dll in search path
    for (auto& path : paths)
    {
        // find wildcard '?'
        int pos = path.find('?');
        if (pos == std::string::npos)
        {
            std::cerr << "Invalid C service path" << std::endl;
            delete mod_info_ptr;
            ::exit(1);
        }

        // mod file path
        std::string mod_path = path.substr(0, pos) + mod_name;

        // load success
        if (mod_info_ptr->dll_loader_.load(mod_path))
            break;
    }

    if (!mod_info_ptr->dll_loader_.is_loaded())
    {
        std::cerr << "try open c service mod(" << mod_name << ") failed : " << mod_info_ptr->dll_loader_.error() << std::endl;
        delete mod_info_ptr;
        return nullptr;
    }

    return mod_info_ptr;
}

}

