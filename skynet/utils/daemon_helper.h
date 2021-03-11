#pragma once

namespace skynet {

// daemon utils
class daemon_helper final
{
public:
    // initialize when create daemon process (create process pid file)
    static bool init(const char* pidfile);
    // clean when daemon process exit (delete process pid file)
    static bool fini(const char* pidfile);
};


}

