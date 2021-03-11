#include "skynet_node_thread.h"

#include "server.h"
#include "skynet_node.h"
#include "skynet_monitor.h"
#include "skynet_socket.h"

#include "../mq/mq.h"
#include "../timer/timer_manager.h"
#include "../context/handle_manager.h"

#include "../utils/signal_helper.h"

#include <iostream>
#include <thread>
#include <csignal>

namespace skynet {

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

// worker thread weight: it determines the processing frequency of messages in the message queue
// -1: 每帧只处理mq中一个消息包 process one message in message queue per update frame
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
void server_thread::start(int thread_count)
{
    // 注册SIGHUP信号处理器, 用于处理 log 文件 reopen
    signal_helper::handle_sighup(&handle_hup);

    // actually thread count: worker thread count + 3 (1 monitor thread, 1 timer thread, 1 socket thread)
    std::shared_ptr<std::thread> threads[thread_count + 3];

    //
    auto m = std::make_shared<server_thread::monitor>();
    m->thread_count = thread_count;
    m->sleep_count = 0;

    // worker thread monitor array
    m->sm.reset(new skynet_monitor[thread_count], std::default_delete<skynet_monitor[]>());

    // start mointer, timer, socket threads
    threads[0] = std::make_shared<std::thread>(server_thread::thread_monitor, m);
    threads[1] = std::make_shared<std::thread>(server_thread::thread_timer, m);
    threads[2] = std::make_shared<std::thread>(server_thread::thread_socket, m);

    // start worker threads
    int weight = 0;
    for (int idx = 0; idx < thread_count; idx++)
    {
        weight = idx < WORKER_THREAD_WEIGHT_COUNT ? WORKER_THREAD_WEIGHT[idx] : 0;
        
        //
        threads[idx + 3] = std::make_shared<std::thread>(server_thread::thread_worker, m, idx, weight);
    }

    // wait all thread exit
    for (auto& thread : threads)
    {
        thread->join();
    }
}

// socket thread proc，并唤醒阻塞的thread_worker线程
void server_thread::thread_socket(std::shared_ptr<monitor> m)
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
            if (skynet_node::instance()->total_svc_ctx() == 0)
                break;

            continue;
        }

        // ret > 0, warkup work thread to process socket message
        if (m->sleep_count >= m->thread_count)
            m->cond.notify_one();
    }
}

void server_thread::thread_monitor(std::shared_ptr<monitor> m)
{
    //
    int n = m->thread_count;
    for (;;)
    {
        // check abort
        if (skynet_node::instance()->total_svc_ctx() == 0)
            break;

        // check dead lock or blocked
        for (int i = 0; i < n; i++)
        {
            m->sm.get()[i].check();
        }

        // total sleep 5 seconds
        for (int i = 0; i < 5; i++)
        {
            // check abort per 1 second
            if (skynet_node::instance()->total_svc_ctx() == 0)
                break;

            // sleep 1 second
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
}

// timer thread proc
void server_thread::thread_timer(std::shared_ptr<monitor> m)
{
    for (;;)
    {
        //
        timer_manager::instance()->update_time();
        // update socket server time
        skynet_socket_updatetime();

        // check abort
        if (skynet_node::instance()->total_svc_ctx() == 0)
            break;

        // notify worker thread
        if (m->sleep_count >= 1)
            m->cond.notify_one();

        // check once per 2.5ms
        std::this_thread::sleep_for(std::chrono::microseconds(2500));
        // SIGHUP
        if (SIG != 0)
        {
            // make log file reopen
            skynet_message msg;
            msg.source = 0;
            msg.session = 0;
            msg.data = nullptr;
            // msg.sz = (size_t)PTYPE_SYSTEM << MESSAGE_TYPE_SHIFT;
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
    std::unique_lock<std::mutex> lock(m->mutex);
    m->is_quit = true;
    m->cond.notify_all();
}

void server_thread::thread_worker(std::shared_ptr<monitor> m, int idx, int weight)
{
    skynet_monitor& sm = m->sm.get()[idx];

    message_queue* q = nullptr;
    while (!m->is_quit)
    {
        // dispatch message
//         q = skynet_context_message_dispatch(sm, q, weight);
        if (q == nullptr)
        {
            std::unique_lock<std::mutex> lock(m->mutex);
            
            ++m->sleep_count;
            if (!m->is_quit)
                m->cond.wait(lock);
            --m->sleep_count;
        }
    }
}

}
