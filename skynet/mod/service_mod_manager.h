#pragma once

#include "../mod/cservice_mod_i.h"
#include "../mod/dll_loader.h"

#include <cassert>
#include <mutex>
#include <array>
#include <unordered_map>

namespace skynet {

// forward declare
class service_context;

// c service mod load info
struct service_mod_info
{
public:
    std::string                     name_ = "";                     // c service mod file name (include file extension .so)
    dll_loader                      dll_loader_;                    // c service mod dll loader

    // mod api
    create_cservice_proc            create_func_ = nullptr;         // c service mod api - create function
    release_cservice_proc           release_func_ = nullptr;        // c service mod api - release function
};

/**
 * C service mod manager
 * manage c service mod, such as snlua, logger, gate etc.
 */
class service_mod_manager final
{
    // singleton
private:
    static service_mod_manager* instance_;
public:
    static service_mod_manager* instance();

private:
    std::string                                 search_path_ = "";          // c service mod .so file search path.
                                                                            // specified by the cservice_path of config file. divide by ';', wildcard: '?'
                                                                            // for example: cservice_path = root.."skynet/cservice/?.so;" .. root.."user/cservice/?.so"

    std::mutex                                  mutex_;                     // protect mod loaded array & count
    std::unordered_map<std::string, service_mod_info*>  service_mod_map_;   // c service mod load info map

public:
    bool init(std::string search_path);
    void fini();

public:
    // query c service mod (try to load if mod not exists)
    service_mod_info* query(std::string mod_name);
    // load c service mod
    service_mod_info* load(std::string mod_name);

private:
    //
    service_mod_info* _try_load_dll(const std::string& mod_name);
};

}


