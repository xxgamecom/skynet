namespace skynet {

inline uint16_t uri_port::value() const
{
    return value_;
}

inline uri_port uri_port::from_string(const std::string& str)
{
    std::regex r("^.*:(\\d+).*$");
    std::smatch m;

    if (std::regex_match(str, m, r))
    {
        std::string port_str = m[1];
        uint16_t val = ::atoi(port_str.c_str());
        return uri_port(val);
    }
    else
    {
        return uri_port();
    }
}

inline std::string uri_port::to_string(const uri_port& port)
{
    if (port.value() == 0) return "";

    std::stringstream ss;
    ss << ":" << port.value();

    return ss.str();
}

}

