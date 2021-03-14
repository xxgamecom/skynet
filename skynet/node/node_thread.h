#pragma once

#include <mutex>
#include <condition_variable>

namespace skynet {

class service_monitor;

// server thread manager (timer, monitor, socket, work thread)
class node_thread final
{
public:
    // service work thread status monitor
    struct monitor
    {
        int                                 thread_count = 0;           // worker thread count
        std::shared_ptr<service_monitor>    svc_monitor;                // worker thread skynet_moniter array
        
        int                                 sleep_count = 0;            // number of sleep worker threads
        bool                                is_quit = false;            // thread quit flag

        //
        std::mutex                          mutex;                      // 
        std::condition_variable             cond;                       // 
    };

public:
    // start threads
    static void start(int thread_count);

    // node thread routine (timer, monitor, socket, worker thread)
private:
    // socket thread proc
    static void thread_socket(std::shared_ptr<monitor> m);
    // monitor thread proc
    static void thread_monitor(std::shared_ptr<monitor> m);
    // timer thread proc
    static void thread_timer(std::shared_ptr<monitor> m);
    // worker thread proc
    static void thread_worker(std::shared_ptr<monitor> m, int idx, int weight);
};


}
