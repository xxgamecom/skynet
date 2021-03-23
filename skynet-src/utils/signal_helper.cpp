#include "signal_helper.h"

namespace skynet {

void signal_helper::ignore_sigpipe()
{
    struct sigaction sa;
    sa.sa_handler = SIG_IGN;
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);
    ::sigaction(SIGPIPE, &sa, 0);
}

void signal_helper::handle_sighup(signal_handler handler)
{
    struct sigaction sa;
    sa.sa_handler = handler;
    sa.sa_flags = SA_RESTART;
    sigfillset(&sa.sa_mask);
    ::sigaction(SIGHUP, &sa, nullptr);
}

}

