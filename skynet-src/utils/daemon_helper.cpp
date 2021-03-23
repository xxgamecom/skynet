#include "daemon_helper.h"

#include <iostream>
#include <csignal>
#include <cerrno>

#include <unistd.h>
#include <fcntl.h>

namespace skynet {

// check the process pid file
int _check_pid(const char* pid_file)
{
    // open process pid file
    FILE* f = ::fopen(pid_file, "r");
    if (f == nullptr)
        return -1;

    // get process pid
    int pid = 0;
    int n = ::fscanf(f, "%d", &pid);

    ::fclose(f);

    if (n != 1 || pid <= 0 || pid == ::getpid())
    {
        return -1;
    }

    if (::kill(pid, 0) && errno == ESRCH)
        return -1;

    return pid;
}

// write process pid file
int _write_pid(const char* pid_file)
{
    // 
    int fd = ::open(pid_file, O_RDWR|O_CREAT, 0644);
    if (fd == -1) 
    {
        std::cerr << "Can't create pidfile: [" << pid_file << "]." << std::endl;
        return -1;
    }

    FILE* f = ::fdopen(fd, "w+");
    if (f == nullptr) 
    {
        std::cerr << "Can't open pidfile: [" << pid_file << "]." << std::endl;
        return -1;
    }

    int pid = -1;
    if (::flock(fd, LOCK_EX|LOCK_NB) == -1) 
    {
        int n = ::fscanf(f, "%d", &pid);
        ::fclose(f);
        if (n != 1)
        {
            std::cerr << "Can't lock and read pidfile." << std::endl;
        } 
        else 
        {
            std::cerr << "Can't lock pidfile, lock is held by pid " << pid << "." << std::endl;
        }
        return -1;
    }

    pid = ::getpid();
    if (::fprintf(f, "%d\n", pid) == 0)
    {
        std::cerr << "Can't write pid." << std::endl;
        ::close(fd);
        return -1;
    }
    ::fflush(f);

    return pid;
}

// redirect stdin, stdout, stderr to /dev/null
bool _redirect_fds()
{
    int nfd = ::open("/dev/null", O_RDWR);
    if (nfd == -1)
    {
        ::perror("Unable to open /dev/null: ");
        return false;
    }
    if (::dup2(nfd, 0) < 0)
    {
        ::perror("Unable to dup2 stdin(0): ");
        return false;
    }
    if (::dup2(nfd, 1) < 0)
    {
        ::perror("Unable to dup2 stdout(1): ");
        return false;
    }
    if (::dup2(nfd, 2) < 0)
    {
        ::perror("Unable to dup2 stderr(2): ");
        return false;
    }

    ::close(nfd);

    return true;
}

bool daemon_helper::init(const char* pid_file)
{
    int pid = _check_pid(pid_file);
    if (pid == -1)
    {
        std::cerr << "Skynet is already running, pid = " << pid << "." << std::endl;
        return false;
    }

#ifdef __APPLE__
    std::cerr << "'daemon' is deprecated: first deprecated in OS X 10.5 , use launchd instead." << std::endl;
#else
    if (::daemon(1, 1))
    {
        std::cerr << "Can't daemonize." << std::endl;
        return false;
    }
#endif

    if (_write_pid(pid_file) <= 0)
    {
        return false;
    }

    // redirect stdin, stdout, stderr
    if (!_redirect_fds())
    {
        return false;
    }

    return true;
}

bool daemon_helper::fini(const char* pid_file)
{
    // delete pid file
    return ::unlink(pid_file) != -1;
}

}

