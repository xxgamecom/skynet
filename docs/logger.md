# skynet日志相关的配置项

优化 log api: skynet.log_info / skynet.log_warn / skynet.log_error / skynet.log_obj / skynet.log_net 等 api

### logger 配置
```lua
logger = {
    -- base
    type = "console_color, daily",          -- "null", "console", "console_color", "hourly", "daily", "rotating", "[console|console_color], [hourly|daily|rotating]"
    log_level = "info",                     -- info, warn, error, off
    file_basename = "./logs/skynet.log",    -- log basename (include file extension)

    -- rotating log file
    rotating = {
        max_size = 80,                      -- 80MB
        max_files = 3,                      --
    },

    -- daily log file
    daily = {
        rotating_hour = 23,                 --
        rotation_minute = 59,               --
    },

}
```

log_path   
仅用于 service_log 的输出目录, debug_console中使用logon/logoff开启和关闭service的专属日志  


