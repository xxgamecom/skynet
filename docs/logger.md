skynet日志相关的配置项

log_path   
仅用于 service_log 的输出目录, debug_console中使用logon/logoff开启和关闭service的专属日志  

log_file  
用于配置 `console` | `file` | `lua logger service mod`;   
nil或为空时, 输出到console; 为日志文件路径, 输出到给定的日志文件;   
如果指定log_service配置项为snlua时, 该配置项为 `lua logger service mod`  

log_service  
`logger` 服务, 为nil时, 默认使用skynet自带的 logger C服务模块;  
如果需要使用自己编写的lua logger 服务模块, 将该参数设置为 snlua，并设置log_file为你的lua logger模块名

当前的日志系统配置太乱了, 需要对logger部分重构


TODO  
refactor logger c service mod : 
支持 log path  
支持 log level     
优化 log api: skynet.log_info / skynet.log_warn / skynet.log_error / skynet.log_obj / skynet.log_net 等 api
优化 log 配置
```lua
logger = {
    -- base
    log_path = "./",
    rotation = true,


    -- log level
    log_level_info = true,
    log_level_warn = true,
    log_level_error = true,
    log_level_obj = true,
    log_level_net = true,
}
```

