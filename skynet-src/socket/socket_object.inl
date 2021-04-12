namespace skynet {

inline bool socket_object::is_invalid(int socket_id)
{
    return (this->socket_id != socket_id || this->status == SOCKET_STATUS_INVALID);
}

inline bool socket_object::is_send_buffer_empty()
{
    return (wb_list_high.head == nullptr && wb_list_low.head == nullptr);
}

inline bool socket_object::nomore_sending_data()
{
    return (is_send_buffer_empty() && dw_buffer == nullptr && (sending & 0xFFFF) == 0) || is_close_write();
}

inline bool socket_object::can_direct_write(int socket_id)
{
    return this->socket_id == socket_id &&
           nomore_sending_data() &&
           status == SOCKET_STATUS_CONNECTED &&
           udp_connecting == 0;
}

inline void socket_object::shutdown_read()
{
    this->status = SOCKET_STATUS_HALF_CLOSE_READ;
}

inline void socket_object::shutdown_write()
{
    this->status = SOCKET_STATUS_HALF_CLOSE_WRITE;
}

inline bool socket_object::is_close_read()
{
    return this->status == SOCKET_STATUS_HALF_CLOSE_READ;
}

inline bool socket_object::is_close_write()
{
    return this->status == SOCKET_STATUS_HALF_CLOSE_WRITE;
}

inline void socket_object::stat_recv(int bytes, uint64_t time_ticks)
{
    io_statistics.recv_bytes += bytes;
    io_statistics.recv_time_ticks = time_ticks;
}

inline void socket_object::stat_send(int bytes, uint64_t time_ticks)
{
    io_statistics.send_bytes += bytes;
    io_statistics.send_time_ticks = time_ticks;
}

}
