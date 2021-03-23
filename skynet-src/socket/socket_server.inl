namespace skynet {

inline void socket_server::update_time(uint64_t time)
{
    time_ = time;
}

inline void socket_server::get_socket_info(std::list<socket_info>& si_list)
{
    socket_pool_.get_socket_info(si_list);
}

}
