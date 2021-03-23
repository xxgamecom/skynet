namespace skynet {

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

inline bool node::is_profile()
{
    return profile_;
}

}
