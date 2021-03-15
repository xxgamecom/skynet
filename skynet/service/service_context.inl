namespace skynet {

inline void service_context::grab()
{
    ++ref_;
}

inline uint32_t service_context::handle()
{
    return svc_handle_;
}

inline void service_context::callback(void* ud, skynet_cb cb)
{
    cb_ = cb;
    cb_ud_ = ud;
}

}
