namespace skynet {

inline void service_manager::svc_inc()
{
    ++svc_count_;
}

inline void service_manager::svc_dec()
{
    --svc_count_;
}

inline int service_manager::svc_count()
{
    return svc_count_;
}

}
