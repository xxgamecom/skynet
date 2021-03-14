#pragma once

#include "node_config.h"

#include <atomic>

namespace skynet {

// forward declare
class node_config;

// server node
class node final
{
private:
    static node* instance_;
public:
    static node* instance();

    // node info
private:
    node_config                 node_config_;               // skynet node config

    std::atomic<int>            total_ { 0 };               // servcie context count in this skynet node
    int                         init_;                      // 是否已初始化, 1表示已经初始化
    uint32_t                    monitor_exit_;              // monitor exit service handle
    bool                        profile_;                   // 是否开启性能统计计数, 默认关闭

private:
    node() = default;
public:
    ~node() = default;

public:
    // initialize skynet node 
    bool init(const std::string config_filename);
    // clean skynet node 
    void fini();

    // start skynet node
    void start();

public:
    // get service ctx count
    int total_svc_ctx();
    // inc service ctx count
    void inc_svc_ctx();
    // dec service ctx count
    void dec_svc_ctx();

    //
    uint32_t get_monitor_exit();
    void set_monitor_exit(uint32_t monitor_exit);

    // 
    void enable_profiler(int enable);
    bool is_profile();
};

}

#include "node.inl"
