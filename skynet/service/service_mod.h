#pragma once

#include <string>

namespace skynet {

// forward declare
class service_context;

// define c service mod APIs
typedef int (*mod_init_proc)(void* inst, service_context* svc_ctx, const char* param);
typedef void* (*mod_create_proc)();
typedef void (*mod_release_proc)(void* inst);
typedef void (*mod_signal_proc)(void* inst, int signal);

// c service mod
class service_mod
{
public:
    std::string                     name = "";                      // c service mod file name (include file extension .so)
    void*                           dll_handle = nullptr;           // c service mod dll handle

    // mod APIs
    mod_init_proc                   init_func = nullptr;            // c service mod api - init function (must)
    mod_create_proc                 create_func = nullptr;          // c service mod api - create function
    mod_release_proc                release_func = nullptr;         // c service mod api - release function
    mod_signal_proc                 signal_func = nullptr;          // c service mod api - signal function

public:
    bool is_dll_loaded();
    bool is_api_loaded();

    // c service mod APIs
public:
    void* instance_create();
    int instance_init(void* inst, service_context* svc_ctx, const char* param);
    void instance_release(void* inst);
    void instance_signal(void* inst, int signal);    
};

}

#include "service_mod.inl"

