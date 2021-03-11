#pragma once

#include <mutex>
#include <condition_variable>

namespace skynet {

class skynet_monitor;

// server thread manager
class server_thread final
{
public:
    // thread status moniter
    struct monitor
    {
        int                                 thread_count = 0;           // worker thread count
        std::shared_ptr<skynet_monitor>     sm;                         // worker thread skynet_moniter array
        
        int                                 sleep_count = 0;            // number of sleep worker threads
        bool                                is_quit = false;            // thread quit flag

        //
        std::mutex                          mutex;                      // 
        std::condition_variable             cond;                       // 
    };

public:
    // start threads
    static void start(int thread_count);

    // thread routine
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
