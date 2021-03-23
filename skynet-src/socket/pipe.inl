namespace skynet {

inline int pipe::read_fd()
{
    return read_fd_;
}

inline int pipe::write_fd()
{
    return write_fd_;
}

}
