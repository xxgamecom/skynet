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
 * 3) c service mod search path set by config file's cpath variable.
 *
 * example:
 * for c service mod logger.so
 * logger_create, logger_init, logger_release, logger_signal
 * 
 * config file
 * cpath=root.."skynet/cservice/?.so;" .. root.."user/cservice/?.so"
 */
#pragma once

#include <dlfcn.h>

#include <cassert>
#include <mutex>
#include <array>

namespace skynet {


// forward declare
struct skynet_context;

// define C service mod APIs
typedef int (*mod_init_proc)(void* inst, skynet_context* ctx, const char* param);
typedef void* (*mod_create_proc)();
typedef void (*mod_release_proc)(void* inst);
typedef void (*mod_signal_proc)(void* inst, int signal);

// dll handle
typedef void* dll_handle;

// c service mod
class service_module
{
public:
    std::string                     name = "";                      // c service mod file name (include file extension .so)
    dll_handle                      handle = nullptr;               // c service mod dll handle

    // mod APIs
    mod_init_proc                   init_func = nullptr;            // c service mod api - init function (must)
    mod_create_proc                 create_func = nullptr;          // c service mod api - create function
    mod_release_proc                release_func = nullptr;         // c service mod api - release function
    mod_signal_proc                 signal_func = nullptr;          // c service mod api - signal function

public:
    bool is_dll_loaded();
    bool is_api_loaded();
};


// c service mod manager
class module_manager final
{
    // singleton
private:
    static module_manager* instance_;
public:
    static module_manager* instance();

private:
    enum
    {
        MAX_MODULE_NUM                          = 32,                   // max c service mod
    };

private:
    std::string                                 search_path_ = "";      // c service mod .so file search path. 
                                                                        // specified by the cpath of config file. divide by ';', wildcard: '?'
                                                                        // for example: cpath = root.."skynet/cservice/?.so;" .. root.."user/cservice/?.so"

    std::mutex                                  mutex_;                 // protect mod loaded array & count
    int                                         count_ = 0;             // loaded c service mod count
    std::array<service_module, MAX_MODULE_NUM>  service_mods_;          // store loaded c service mods

public:
    // initialize
    // @param path c service mod search path
    bool init(const std::string search_path);

    // add c service mod
    bool add(service_module* mod);
    // query c service mod (try to load if mod not exists)
    service_module* query(const std::string mod_name);

    // c service mod APIs
public:
    void* instance_create(service_module* mod);
    int instance_init(service_module* mod, void* inst, skynet_context* ctx, const char* param);
    void instance_release(service_module* mod, void* inst);
    void instance_signal(service_module* mod, void* inst, int signal);

private:
    // find mod in loaded mods array
    service_module* _find_loaded_mod(const std::string& mod_name);

    // try to load mod_name.so file
    dll_handle _try_load_mod(const std::string& mod_name);

    // load c service mod APIs
    static bool _load_mod_api(service_module& mod);

    // get api
    template<typename SYMBOL>
    static SYMBOL _get_symbol(service_module& mod, std::string symbol);
};

}

// include inline methods
#include "module_manager.inl"

