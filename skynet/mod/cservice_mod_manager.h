/**
 * C service mod manager
 * manage c service mod, such as snlua, logger, gate etc.
 *
 * C service mod specs:
 * 1) have 4 APIs: 
 *    - xxx_create:  used to create c service instance.
 *    - xxx_init:    used to initialize c service instance, it's mainly used to set message callback.
 *    - xxx_release: used to release c service instance.
 *    - xxx_signal:  called when a signal is triggered.
 * 2) xxx_init api must implement, other apis are optional;
 * 3) c service mod search path set by config file's cservice_path variable.
 *
 * example:
 * for c service mod logger.so
 * logger_create, logger_init, logger_release, logger_signal
 * 
 * config file
 * cservice_path = root.."skynet/cservice/?.so;" .. root.."user/cservice/?.so"
 */
#pragma once

#include "cservice_mod.h"

#include "../utils/dll_helper.h"

#include <cassert>
#include <mutex>
#include <array>

namespace skynet {

// forward declare
struct skynet_context;

// c service mod manager
class cservice_mod_manager final
{
    // singleton
private:
    static cservice_mod_manager* instance_;
public:
    static cservice_mod_manager* instance();

private:
    enum
    {
        MAX_MODULE_NUM                          = 32,                   // max c service mod
    };

private:
    std::string                                 search_path_ = "";      // c service mod .so file search path. 
                                                                        // specified by the cservice_path of config file. divide by ';', wildcard: '?'
                                                                        // for example: cservice_path = root.."skynet/cservice/?.so;" .. root.."user/cservice/?.so"

    std::mutex                                  mutex_;                 // protect mod loaded array & count
    int                                         count_ = 0;             // loaded c service mod count
    std::array<cservice_mod, MAX_MODULE_NUM>    cservice_mods_;         // store loaded c service mods

public:
    // initialize
    // @param path c service mod search path
    bool init(const std::string search_path);

    // add c service mod
    bool add(cservice_mod* mod);
    // query c service mod (try to load if mod not exists)
    cservice_mod* query(const std::string mod_name);

private:
    // find mod in loaded mods array
    cservice_mod* _find_loaded_mod(const std::string& mod_name);

    // try to load c service mod_name.so file
    void* _try_load_mod_dll(const std::string& mod_name);
    // try to load c service mod APIs
    static bool _try_load_mod_api(cservice_mod& mod);
};

}

// include inline methods
#include "cservice_mod_manager.inl"

