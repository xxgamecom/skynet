Skynet++
==========

**Skynet++ Framework Architecture**
![](docs/framework_architecture.jpg "")

## 目录结构
```
skynet++
├── CMakeLists.txt                  // CMake文件
├── README.md                       // 项目说明文档
├── bin                             // 编译输出目录 (编译后生成)
│   ├── lua                         // lua命令行解释器
│   ├── luac                        // lua编译工具
│   ├── cservice                    // skynet c服务 编译输出目录
│   │   ├── gate.so                 // `gate服务` 编译后的生成文件, 该服务是一个用于 
│   │   └── snlua.so                // `snlua服务` 编译后的生成文件, 该服务是一个用于启动lua服务的c服务
│   ├── luaclib                     // skynet lua c模块编译输出目录
│   │   ├── bson.so                 // lua-bson模块编译生成文件
│   │   ├── cjson.so                // lua-cjson模块编译生成文件
│   │   ├── codec.so                // ...
│   │   ├── httpsc.so
│   │   ├── iconv.so
│   │   ├── lfs.so
│   │   ├── lpeg.so
│   │   ├── luacurl.so
│   │   ├── md5.so
│   │   ├── protobuf.so
│   │   ├── snapshot.so
│   │   └── sproto.so
│   └── skynet                      // skynet可执行文件
├── cmake                           // cmake相关功能
├── examples                        // 示例
├── skynet-service-c                // skynet c服务 目录
├── skynet-deps                     // skynet 依赖目录
│   ├── jemalloc                    // jemalloc库
│   └── lua                         // lua库
├── skynet-luaclib                  // skynet lua c模块源码目录
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
│   ├── lua-tls                     // ltls模块, 依赖OpenSSL
│   └── sproto                      // sproto模块
├── skynet-lualib                   // skynet lua模块 源码目录
└── skynet                          // skynet目录, 最底层的skynet actor模型等的实现
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
Skynet++使用CMake构建。

1. 创建目录
```shell
$ mkdir skynet
$ cd skynet
```

2. 从github拉取项目
```shell
$ git clone https://github.com/xxgame/skynet++
```

3. 更新子模块
```shell
$ git submodule update --init
```

4. 创建cmake构建目录
```shell
$ cd ../
$ mkdir cmake-build && cd cmake-build
```

5. 编译
```shell
$ cmake ../skynet++ -DOPENSSL_ROOT_DIR=/usr/local/opt/openssl
$ make
```
