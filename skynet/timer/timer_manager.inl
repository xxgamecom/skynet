namespace skynet {

// 返回当前进程启动后经过的时间 (0.01 秒)
inline uint64_t timer_manager::now()
{
    return TI->current;
}

// 返回当前进程的启动 UTC 时间（秒）
inline uint32_t timer_manager::start_time()
{
    return TI->start_time;
}

}
