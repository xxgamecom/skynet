
#include <iostream>
#include <algorithm>
#include <regex>

#include <dlfcn.h>

std::string search_path_ = "../bin/?.so;../bin/luaclib/?.so";

void* try_load_mod(const std::string& mod_name)
{
    // notice: search path divide by ';' wildcard: '?'

    // split by ';'
    std::regex re {';'};
    std::vector<std::string> paths {
        std::sregex_token_iterator(search_path_.begin(), search_path_.end(), re, -1),
        std::sregex_token_iterator()
    };

    // load mod_name.so
    void* handle = nullptr;
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
    else
    {
        std::cout << "load " << mod_name << " success" << std::endl;
    }

    return handle;
}

int main()
{
    void* h = nullptr;
    
    // test
    h = try_load_mod("bson.so");
    h = try_load_mod("cjson.so");
    h = try_load_mod("a.so");
}

