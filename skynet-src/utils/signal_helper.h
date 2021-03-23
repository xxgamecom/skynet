#pragma once

#include <csignal>

namespace skynet {

//
typedef void (*signal_handler)(int);

// signal utils
class signal_helper final
{
public:
    // ignore SIGPIPE, SIGPIPE default behavior: terminate the process, so need ignore SIGPIPE
    static void ignore_sigpipe();
    // handle SIGHUP signal
    static void handle_sighup(signal_handler handler);
};


}

