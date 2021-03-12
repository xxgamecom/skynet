namespace skynet {

// 服务context数量
inline int node::total_svc_ctx()
{
    return total_;
}

// 增加服务context计数
inline void node::inc_svc_ctx()
{
    ++total_;
}

// 减少服务context计数
inline void node::dec_svc_ctx()
{
    --total_;
}

inline uint32_t node::get_monitor_exit()
{
    return monitor_exit_;
}

inline void node::set_monitor_exit(uint32_t monitor_exit)
{
    monitor_exit_ = monitor_exit;
}

inline void node::enable_profiler(int enable)
{
    profile_ = (bool)enable;
}

}
