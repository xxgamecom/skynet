namespace skynet {

inline bool service_mod::is_dll_loaded()
{
    return dll_handle != nullptr;
}

inline bool service_mod::is_api_loaded()
{
    return init_func != nullptr;
}


inline void* service_mod::instance_create()
{
    if (create_func != nullptr)
        return create_func();
    else
        return (void*)(intptr_t)(~0);
}

inline int service_mod::instance_init(void* inst, service_context* svc_ctx, const char* param)
{
    assert(init_func != nullptr);
    return init_func(inst, svc_ctx, param);
}

inline void service_mod::instance_release(void* inst)
{
    if (release_func != nullptr)
        release_func(inst);
}

inline void service_mod::instance_signal(void* inst, int signal)
{
    if (signal_func != nullptr)
        signal_func(inst, signal);
}

}

