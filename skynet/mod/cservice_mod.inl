namespace skynet {

inline bool cservice_mod::is_dll_loaded()
{
    return dll_handle != nullptr;
}

inline bool cservice_mod::is_api_loaded()
{
    return init_func != nullptr;
}


inline void* cservice_mod::instance_create()
{
    if (create_func != nullptr)
        return create_func();
    else
        return (void*)(intptr_t)(~0);
}

inline int cservice_mod::instance_init(void* inst, skynet_context* ctx, const char* param)
{
    assert(init_func != nullptr);
    return init_func(inst, ctx, param);
}

inline void cservice_mod::instance_release(void *inst)
{
    if (release_func != nullptr)
        release_func(inst);
}

inline void cservice_mod::instance_signal(void* inst, int signal)
{
    if (signal_func != nullptr)
        signal_func(inst, signal);
}

}

