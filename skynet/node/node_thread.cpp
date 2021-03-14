#include "node_thread.h"

#include "node.h"
#include "service_monitor.h"
#include "skynet_socket.h"

#include "../skynet.h"  // api

#include "../mq/mq_private.h"
#include "../mq/mq_msg.h"

#include "../timer/timer_manager.h"

#include "../context/service_context.h"
#include "../context/handle_manager.h"

#include "../utils/signal_helper.h"

#include <iostream>
#include <thread>
#include <csignal>

namespace skynet {

// service work thread status monitor
struct monitor_data
{
    int                                 work_thread_num = 0;            // number of work thread
    std::shared_ptr<service_monitor>    svc_monitors;                   // service work thread monitor array

    int                                 work_thread_sleep_count = 0;    // number of sleep worker threads
    bool                                is_work_thread_quit = false;    // work thread quit flag

    //
    std::mutex                          mutex;                          //
    std::condition_variable             cond;                           //
};

// sighup handle
// SIGHUP 信号在用户终端连接(正常或非正常)结束时发出，通常是在终端的控制进程结束时, 通知同一session内的各个作业
static volatile int SIG = 0;
static void handle_hup(int signal)
{
    if (signal == SIGHUP)
    {
        SIG = 1;
    }
}

// weight of the service worker thread: it determines the processing frequency of messages in the message queue
// -1: process one message in message queue per update frame (每帧只处理mq中一个消息包)
//  0: 每帧处理mq中所有消息包, process all messages in message queue per update frame
//  1: 每帧处理mq长度的1/2条(>>1右移一位)消息, process 1/2 messages in message queue per update frame
//  2: 每帧处理mq长度的1/4(右移2位), process 1/4 messages in message queue per update frame
//  3: 每帧处理mq长度的1/8(右移3位), process 1/8 messages in message queue per update frame
static int WORKER_THREAD_WEIGHT[] = {
    -1, -1, -1, -1,              // 0 - 3
    0, 0, 0, 0,                  // 4 - 7
    1, 1, 1, 1, 1, 1, 1, 1,      // 8 - 15
    2, 2, 2, 2, 2, 2, 2, 2,      // 16 - 23
    3, 3, 3, 3, 3, 3, 3, 3,      // 24 - 31
};

const int WORKER_THREAD_WEIGHT_COUNT = sizeof(WORKER_THREAD_WEIGHT) / sizeof(WORKER_THREAD_WEIGHT[0]);

// start threads
void node_thread::start(int work_thread_num)
{
    // 注册SIGHUP信号处理器, 用于处理 log 文件 reopen
    signal_helper::handle_sighup(&handle_hup);

    // actually thread count: worker thread count + 3 (1 monitor thread, 1 timer thread, 1 socket thread)
    std::shared_ptr<std::thread> threads[work_thread_num + 3];

    //
    auto monitor_data_ptr = std::make_shared<monitor_data>();
    monitor_data_ptr->work_thread_num = work_thread_num;
    monitor_data_ptr->work_thread_sleep_count = 0;

    // worker thread monitor array
    monitor_data_ptr->svc_monitors.reset(new service_monitor[work_thread_num], std::default_delete<service_monitor[]>());

    // start mointer, timer, socket threads
    threads[0] = std::make_shared<std::thread>(node_thread::thread_monitor, monitor_data_ptr);
    threads[1] = std::make_shared<std::thread>(node_thread::thread_timer, monitor_data_ptr);
    threads[2] = std::make_shared<std::thread>(node_thread::thread_socket, monitor_data_ptr);

    // start worker threads
    int weight = 0;
    for (int idx = 0; idx < work_thread_num; idx++)
    {
        weight = idx < WORKER_THREAD_WEIGHT_COUNT ? WORKER_THREAD_WEIGHT[idx] : 0;
        
        //
        threads[idx + 3] = std::make_shared<std::thread>(node_thread::thread_worker, monitor_data_ptr, idx, weight);
    }

    // wait all thread exit
    for (auto& thread : threads)
    {
        thread->join();
    }
}

// socket thread proc，并唤醒阻塞的thread_worker线程
void node_thread::thread_socket(std::shared_ptr<monitor_data> monitor_data_ptr)
{
    int ret = 0;
    for (;;)
    {
        // poll socket message
        ret = skynet_socket_poll();
        
        // exit
        if (ret == 0)
            break;

        // error or has more messages, continue
        if (ret < 0)
        {
            // check abort
            if (node::instance()->total_svc_ctx() == 0)
                break;

            continue;
        }

        // ret > 0, warkup work thread to process socket message
        if (monitor_data_ptr->work_thread_sleep_count >= monitor_data_ptr->work_thread_num)
            monitor_data_ptr->cond.notify_one();
    }
}

void node_thread::thread_monitor(std::shared_ptr<monitor_data> monitor_data_ptr)
{
    int n = monitor_data_ptr->work_thread_num;
    for (;;)
    {
        // check abort
        if (node::instance()->total_svc_ctx() == 0)
            break;

        // check dead lock or blocked
        for (int i = 0; i < n; i++)
        {
            monitor_data_ptr->svc_monitors.get()[i].check();
        }

        // check interval: 5 seconds
        for (int i = 0; i < 5; i++)
        {
            // check abort per 1 second
            if (node::instance()->total_svc_ctx() == 0)
                break;

            // sleep 1 second
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
}

// timer thread proc
void node_thread::thread_timer(std::shared_ptr<monitor_data> monitor_data_ptr)
{
    for (;;)
    {
        //
        timer_manager::instance()->update_time();
        // update socket server time
        skynet_socket_updatetime();

        // check abort
        if (node::instance()->total_svc_ctx() == 0)
            break;

        // notify worker thread
        if (monitor_data_ptr->work_thread_sleep_count >= 1)
            monitor_data_ptr->cond.notify_one();

        // check once per 2.5ms
        std::this_thread::sleep_for(std::chrono::microseconds(2500));

        // check SIGHUP
        if (SIG != 0)
        {
            // reopen log file
            skynet_message msg;
            msg.src_svc_handle = 0;
            msg.session = 0;
            msg.data = nullptr;
            msg.sz = (size_t)message_type::PTYPE_SYSTEM << MESSAGE_TYPE_SHIFT;
            uint32_t logger_svc_handle = handle_manager::instance()->find_by_name("logger");
            if (logger_svc_handle != 0)
            {
                skynet_context_push(logger_svc_handle, &msg);
            }

            SIG = 0;
        }
    }

    // exit socket thread
    skynet_socket_exit();

    // exit all worker thread
    std::unique_lock<std::mutex> lock(monitor_data_ptr->mutex);
    monitor_data_ptr->is_work_thread_quit = true;
    monitor_data_ptr->cond.notify_all();
}

void node_thread::thread_worker(std::shared_ptr<monitor_data> monitor_data_ptr, int idx, int weight)
{
    service_monitor& svc_monitor = monitor_data_ptr->svc_monitors.get()[idx];

    mq_private* q = nullptr;
    while (!monitor_data_ptr->is_work_thread_quit)
    {
        // dispatch message
        q = node::instance()->message_dispatch(svc_monitor, q, weight);
        if (q == nullptr)
        {
            std::unique_lock<std::mutex> lock(monitor_data_ptr->mutex);
            
            ++monitor_data_ptr->work_thread_sleep_count;
            if (!monitor_data_ptr->is_work_thread_quit)
                monitor_data_ptr->cond.wait(lock);
            --monitor_data_ptr->work_thread_sleep_count;
        }
    }
}

}
