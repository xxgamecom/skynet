namespace skynet { namespace socket {

// 计算socket slot数组下标
inline uint32_t socket_server::calc_slot_index(int socket_id)
{
    return (((uint32_t)socket_id) % MAX_SOCKET);
}
// 高16位
inline uint16_t socket_server::socket_id_tag16(int socket_id)
{
    return ((socket_id >> MAX_SOCKET_P) & 0xFFFF);
}

} }
