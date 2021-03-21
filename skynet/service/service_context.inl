namespace skynet {

inline int service_context::new_session()
{
    // session always be a positive number
    int session = ++session_id_;
    if (session <= 0)
    {
        session_id_ = 1;
        return 1;
    }

    return session;
}

inline void service_context::grab()
{
    ++ref_;
}

inline void service_context::set_callback(skynet_cb msg_callback, void* cb_ud/* = nullptr*/)
{
    msg_callback_ = msg_callback;
    cb_ud_ = cb_ud;
}

}
