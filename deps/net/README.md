skynet.network
----------------------------------------------

使用方法  
~~~~~~~~  
1. 主要使用tcp_client和tcp_server这两个类  
2. tcp_client和tcp_server的配置项通过get_config()获得  
3. 请在打开tcp_client和tcp_server前设置这些配置  
4.  


测试用例  
~~~~~~~~  
1. connector  
作用: 用于测试tcp_connector  
工程: test_network_connector  

2. echo  
作用: echo测试  
工程: test_network_echo_client  
      test_network_echo_server  

3. http  
作用: http客户端  
工程: test_network_http_client  

4. stress  
作用: 网络库压力测试  
工程: test_network_stress_client  
      test_network_stress_server  
脚本: run_test_network_stress_client.bat  
    : run_test_network_stress_server.bat  


TODO  
~~~~  
1. tcp_client使用域名地址进行连接时, 内部在域名解析完
成后只对第一个地址进行尝试连接，需要支持在连接失败的情
况下尝试连接其他解析到的地址  
2. 将session_pool移到session_manager中
3. 检查tcp_server中handle_session_close, release_session处理顺序

优化点  
~~~~~~  
1. 会话读写数据部分  

写数据  
现在的write实现内部会使用io_buffer，会有一次内存拷贝，
将来考虑支持直接发送外部缓存，这种方式需要外部维护该
缓存的生命周期，这种方式必须保证在异步发送前缓存有效，
当然，外部缓存的生命周期管理可以交由网络库进行管理，
这样就需要提供一个缓存池接口给外部。

读数据  
现在每个会话读取数据内部使用了一个io_buffer，每次读取
到数据后，会把io_buffer中的数据抛给用户处理。这里无需
优化。  

2. 写消息队列  
目前使用的是std::mutex来对写消息队列进行保护, 未来考虑
是否采用lockfree的方式  


