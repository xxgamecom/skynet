skynet++
==========

## 编写skynet++的原因

* 原版的skynet属于比较底层的实现, 严格的来说还称不上一个框架, 使用的过程还需要做大量的开发工作
* 对C不是太熟悉, 工作中需要对原版skynet进行修改和维护, 使用C++重写
* 原skynet缺少服务治理的支持
* 原skynet上层服务框架不完善
* 

## 与原版skynet的异同

* 移除了分布式harbor模式(harbor模式有点鸡肋), 只支持cluster模式
* 底层skynet使用C++重新编写
* 对服务治理提供支持: 注册服务、发现服务、配置服务等
* 提供简单完善的服务RPC接口
* 

**skynet++ Framework Architecture**
![](docs/framework_architecture.jpg "")

## 目录结构
```
bin                                 // 编译输出目录 (编译后生成)
├── lua                             // lua命令行解释器
├── luac                            // lua编译工具
├── cservice                        // skynet c服务 编译输出目录
│   ├── gate.so                     // `gate服务` 编译后的生成文件, 该服务是一个用于 
│   └── snlua.so                    // `snlua服务` 编译后的生成文件, 该服务是一个用于启动lua服务的c服务
├── luaclib                         // skynet lua c模块编译输出目录
│   ├── bson.so                     // lua-bson模块编译生成文件
│   ├── cjson.so                    // lua-cjson模块编译生成文件
│   ├── protobuf.so                 // pbc模块编译生成文件
│   ├── codec.so                    // ...
│   ├── skynet.so                   // skynet luaclib编译生成文件
│   └── sproto.so
├── skynet                          // skynet可执行文件
├── ...
cmake-build                         // cmake编译目录
├── ...
skynet                              // skynet源码目录
├── CMakeLists.txt                  // CMake文件
├── README.md                       // 项目说明文档
├── cmake                           // cmake相关功能
├── deps                            // skynet 依赖目录
│   ├── jemalloc                    // jemalloc库
│   └── lua                         // lua库
├── examples                        // 示例
├── skynet                          // skynet源码目录, 最底层的skynet actor模型等的实现
│   ├── ...
│   ├── skynet.cpp                  // skynet主文件
│   └── skynet.h                    // skynet API header file
├── skynet-luaclib                  // skynet luaclib 源码目录
│   ├── lua-bson                    // bson模块
│   ├── lua-cjson                   // cjson模块
│   ├── lua-codec                   // 基于OpenSSL实现的codec模块
│   ├── lua-curl                    // curl模块, 依赖OpenSSL
│   ├── lua-epoll                   // epoll模块, 仅用于Linux系统
│   ├── lua-httpsc                  // https client模块, 依赖OpenSSL
│   ├── lua-iconv                   // iconv模块
│   ├── lua-lfs                     // lfs模块
│   ├── lua-lpeg                    // lpeg模块
│   ├── lua-md5                     // md5模块
│   ├── lua-pbc                     // pbc模块
│   ├── lua-snapshot                // snapshot模块
│   ├── lua-skynet                  // skynet luaclib 源码
│   ├── lua-tls                     // ltls模块, 依赖OpenSSL
│   └── sproto                      // sproto模块
├── skynet-lualib                   // skynet lua模块 源码目录
│   ├── ...
├── skynet-service-c                // skynet c service 源码目录
│   ├── gate                        // c service gate
│   ├── logger                      // c service logger
│   └── snlua                       // c service snlua
├── skynet-service-lua              // skynet lua service 源码目录
│   ├── ...
└── tests                           // 单元测试目录
```

## 安装依赖库

1. Mac
```shell
brew install openssl
```

2. Centos
```shell
sudo yum install openssl
```

3. Ubuntu
```shell
sudo apt-get install openssl
sudo apt-get install libssl-dev
```

## 编译
skynet++使用CMake构建。

```shell
# 1 创建编译目录
$ mkdir skynet
$ cd skynet

# 2 从github拉取项目
$ git clone https://github.com/xxgamecom/skynet

# 3 创建cmake构建目录
$ cd ../
$ mkdir cmake-build && cd cmake-build

# 4 编译
$ cmake ../skynet -DOPENSSL_ROOT_DIR=/usr/local/opt/openssl
$ make
```


