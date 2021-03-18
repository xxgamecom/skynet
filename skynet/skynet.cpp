#include "node/node.h"
#include "utils/signal_helper.h"

#include <iostream>

int main(int argc, char* argv[])
{
    // check command line arguments
    if (argc <= 1)
    {
        std::cerr << "Need a config file. Please read skynet wiki : https://github.com/cloudwu/skynet/wiki/Config" << std::endl;
        std::cerr << "Usage: skynet <config_file>" << std::endl;
        return 1;
    }
    // config filename
    std::string config_filename = argv[1];

    // ignore SIGPIPE
    skynet::signal_helper::ignore_sigpipe();

    // initialize skynet node
    if (!skynet::node::instance()->init(config_filename))
    {
        std::cerr << "skynet node initialize failed." << std::endl;
        return 1;
    }

    // start skynet node
    skynet::node::instance()->start();

    // clean skynet node
    skynet::node::instance()->fini();

    return 0;
}
