namespace skynet { namespace socket {

inline bool socket::is_send_buffer_empty()
{
    return (wb_list_high.head == nullptr && wb_list_low.head == nullptr);
}

inline bool socket::nomore_sending_data()
{
    return is_send_buffer_empty() &&
           dw_buffer == nullptr &&
           (sending & 0xFFFF) == 0;
}

inline bool socket::can_direct_write(int socket_id)
{
    return this->socket_id == socket_id &&
           nomore_sending_data() &&
           status == status::CONNECTED &&
           udp_connecting == 0;
}

inline void socket::stat_recv(int n, uint64_t time)
{
    stat.recv += n;
    stat.recv_time = time;
}

inline void socket::stat_send(int n, uint64_t time)
{
    stat.send += n;
    stat.send_time = time;
}


} }
