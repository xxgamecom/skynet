namespace skynet {

inline socket_object& socket_object_pool::get_socket(int socket_id)
{
    return socket_array_[calc_slot_index(socket_id)];
}

inline std::array<socket_object, socket_object_pool::MAX_SOCKET>& socket_object_pool::get_sockets()
{
    return socket_array_;
}

inline uint32_t socket_object_pool::calc_slot_index(int socket_id)
{
    return (((uint32_t)socket_id) % MAX_SOCKET);
}

inline uint16_t socket_object_pool::socket_id_high(int socket_id)
{
    return ((socket_id >> MAX_SOCKET_P) & 0xFFFF);
}

}

