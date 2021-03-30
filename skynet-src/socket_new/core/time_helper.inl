namespace skynet { namespace network {

inline time_helper::system_clock::time_point time_helper::system_now()
{
    return system_clock::now();
}

inline time_helper::steady_clock::time_point time_helper::steady_now()
{
    return steady_clock::now();
}

inline time_helper::high_resolution_clock::time_point time_helper::high_now()
{
    return high_resolution_clock::now();
}

inline time_helper::system_clock::time_point time_helper::from_time_t(std::time_t time)
{
    return system_clock::from_time_t(time);
}

inline std::time_t time_helper::to_time_t(const system_clock::time_point& tp)
{
    return system_clock::to_time_t(tp);
}

} }
