#pragma once

#include <mutex>
#include <condition_variable>

namespace skynet {

// forward declare
struct monitor_data;

// server thread manager (timer, monitor, socket, work thread)
class node_thread final
{
public:
    // start threads
    static void start(int work_thread_num);

    // node thread routine (timer, monitor, socket, worker thread)
private:
    // socket thread proc
    static void thread_socket(std::shared_ptr<monitor_data> monitor_data_ptr);
    // monitor thread proc
    static void thread_monitor(std::shared_ptr<monitor_data> monitor_data_ptr);
    // timer thread proc
    static void thread_timer(std::shared_ptr<monitor_data> monitor_data_ptr);
    // worker thread proc
    static void thread_worker(std::shared_ptr<monitor_data> monitor_data_ptr, int idx, int weight);
};


}
