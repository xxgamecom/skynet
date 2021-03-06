namespace skynet {

inline bool socket_object::is_invalid(int socket_id)
{
    return (this->socket_id != socket_id || socket_status == SOCKET_STATUS_INVALID);
}

inline bool socket_object::is_write_buffer_empty()
{
    return (write_buffer_list_high.head == nullptr && write_buffer_list_low.head == nullptr);
}

inline bool socket_object::nomore_sending_data()
{
    return (is_write_buffer_empty() && direct_write_buffer == nullptr && (sending_count & 0xFFFF) == 0) || is_close_write();
}

inline bool socket_object::can_direct_send(int socket_id)
{
    return this->socket_id == socket_id &&
           nomore_sending_data() &&
           socket_status == SOCKET_STATUS_CONNECTED &&
           udp_connecting_count == 0;
}

inline void socket_object::shutdown_read()
{
    socket_status = SOCKET_STATUS_HALF_CLOSE_READ;
}

inline void socket_object::shutdown_write()
{
    socket_status = SOCKET_STATUS_HALF_CLOSE_WRITE;
}

inline bool socket_object::is_close_read()
{
    return this->socket_status == SOCKET_STATUS_HALF_CLOSE_READ;
}

inline bool socket_object::is_close_write()
{
    return socket_status == SOCKET_STATUS_HALF_CLOSE_WRITE;
}

inline void socket_object::statistics_recv(int bytes, uint64_t time_ticks)
{
    io_statistics.recv_bytes += bytes;
    io_statistics.recv_time_ticks = time_ticks;
}

inline void socket_object::statistics_send(int bytes, uint64_t time_ticks)
{
    io_statistics.send_bytes += bytes;
    io_statistics.send_time_ticks = time_ticks;
}

}
