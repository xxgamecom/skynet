#pragma once

namespace skynet {

// daemon utils
class daemon_helper final
{
public:
    // initialize when create daemon process (create process pid file)
    static bool init(const char* pid_file);
    // clean when daemon process exit (delete process pid file)
    static bool fini(const char* pid_file);
};


}

