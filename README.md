# muduox

> 从零实现的跨平台 muduo 风格高性能 C++20 网络库

muduox 是对陈硕 [muduo](https://github.com/chenshuo/muduo) 网络库的复刻与扩展，在保留 Reactor 架构精髓的基础上，**原生支持 Linux（epoll）和 Windows（IOCP）双平台**。

## 架构

```
                           +------------------+
                           |    TcpServer     |     用户层
                           +--------+---------+
                                    |
                           +--------+---------+
                           |   TcpConnection  |     连接层
                           +--------+---------+
                                    |
                    +---------------+---------------+
                    |                               |
            +-------+--------+             +--------+-------+
            |   Acceptor      |             |  EventLoop     |   事件循环
            +-------+--------+             +--------+-------+
                    |                               |
            +-------+--------+             +--------+-------+
            |   Socket        |             |  Poller (抽象)  |   IO 多路复用
            +-------+--------+             +--------+-------+
                                           /                \
                                 +--------+------+   +------+--------+
                                 | EpollPoller   |   | IocpPoller    |
                                 | (Linux/macOS) |   | (Windows)     |
                                 +---------------+   +---------------+

                +--------+--------+
                |  SocketOps      |     跨平台 socket 操作
                +--------+--------+
                |  Buffer         |     应用层缓冲区
                +--------+--------+
                |  Channel        |     事件分发
                +-----------------+
```

**Reactor + one loop per thread** — 每个线程一个 EventLoop，通过 Poller 监听 IO 事件，Channel 分发到上层回调。

### epoll vs IOCP

| 特性 | epoll (Linux) | IOCP (Windows) |
|------|---------------|----------------|
| 模式 | Reactor：等事件 → 自己 read/write | Proactor → Reactor 适配 |
| accept | `accept4()` 直接拿 fd | `AcceptEx` 异步接受 |
| recv | `read()` / `recv()` 同步读 | `WSARecv` 异步读 |
| 事件通知 | `epoll_wait` | `GetQueuedCompletionStatus` |

IOCP 下通过 `IocpContext` 预投递异步 IO，完成时翻译为 Reactor 事件，保持上层接口一致。

## 当前进度

- **[x] 阶段一** — 项目骨架 + CMake 跨平台构建
- **[x] 阶段二** — Socket 层（SocketOps + Socket + InetAddress）
- **[x] 阶段三** — Reactor 核心（Channel + Poller + EpollPoller + EventLoop）
- **[x] 阶段四** — 网络层（Acceptor + Buffer + TcpConnection + TcpServer）
- **[ ] 阶段五** — IOCP Poller（Windows 异步 IO）
- **[ ] 阶段六** — 扩展（EventLoopThreadPool、TimerQueue、TcpClient、HTTP 模块）

## 项目结构

```
muduox/
├── CMakeLists.txt                    # 构建配置
├── plan.md                           # 实现计划
├── README.md
├── muduox/
│   ├── base/
│   │   ├── noncopyable.h             # 禁止拷贝 mixin
│   │   └── copyable.h                # 可拷贝 tag
│   └── net/
│       ├── Callbacks.h               # 回调类型定义
│       ├── InetAddress.h/.cpp        # IP 地址 + 端口封装
│       ├── SocketOps.h/.cpp          # 跨平台 socket 操作（#ifdef _WIN32）
│       ├── Socket.h/.cpp             # Socket RAII 封装
│       ├── Channel.h/.cpp            # 事件分发单元（fd + 回调）
│       ├── Poller.h/.cpp             # IO 多路复用抽象基类
│       ├── EpollPoller.h/.cpp        # Linux epoll 实现
│       ├── EventLoop.h/.cpp          # 事件循环 + wakeup 机制
│       ├── Acceptor.h/.cpp           # 监听器（accept + Channel）
│       ├── Buffer.h/.cpp             # 应用层缓冲区（prependable/readable/writable）
│       ├── TcpConnection.h/.cpp      # TCP 连接（enable_shared_from_this）
│       └── TcpServer.h/.cpp          # TCP 服务器
└── examples/
    └── echo/
        ├── main.cpp                  # Echo Server（端口 8888）
        └── client.cpp                # Echo Client（非阻塞 connect + 自动验证）
```

## 快速开始

### 构建

**Linux / macOS：**

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

**Windows（MSYS2 / MinGW）：**

```bash
mkdir build && cd build
cmake .. -G "MinGW Makefiles"
cmake --build .
```

**Windows（Visual Studio）：**

```powershell
mkdir build; cd build
cmake .. -G "Visual Studio 17 2022"
cmake --build . --config Release
```

### 运行 Echo Server

```bash
# 终端 1：启动服务器
./build/echo_server
# 输出：Echo server listening on 0.0.0.0:8888
```

### 运行 Echo Client

```bash
# 终端 2：启动客户端（自动连接、发送、验证）
./build/echo_client
# 输出：
#   Connecting to 127.0.0.1:8888 ...
#   Starting event loop...
#   Connected. Sending: Hello echo server!
#   Received: Hello echo server!
#   PASS: echo matches!
```

### 代码示例

下面是一个完整的 Echo Server 示例：

```cpp
#include "muduox/net/EventLoop.h"
#include "muduox/net/TcpServer.h"
#include "muduox/net/InetAddress.h"
#include "muduox/net/SocketOps.h"
#include <iostream>

int main() {
    muduox::sockets::startup();  // 平台初始化

    muduox::EventLoop loop;
    muduox::InetAddress listenAddr(8888);
    muduox::TcpServer server(&loop, listenAddr, "EchoServer");

    server.setConnectionCallback([](const muduox::TcpConnectionPtr& conn) {
        std::cout << "[" << conn->peerAddress().toIpPort()
                  << "] " << (conn->connected() ? "UP" : "DOWN")
                  << std::endl;
    });

    server.setMessageCallback([](const muduox::TcpConnectionPtr& conn,
                                  muduox::Buffer* buf) {
        conn->send(buf->peek(), buf->readableBytes());
        buf->retrieveAll();
    });

    server.start();
    std::cout << "Echo server listening on 0.0.0.0:8888" << std::endl;
    loop.loop();

    muduox::sockets::cleanup();
    return 0;
}
```

## 技术特性

- **C++20** — 使用 `std::any`、`std::jthread`、`std::span` 等现代特性
- **Reactor 模式** — 非阻塞 IO + IO 多路复用，统一事件分发
- **one loop per thread** — 每个线程独立事件循环，无线程安全开销
- **跨平台** — `#ifdef _WIN32` 条件编译，Linux 用 epoll、Windows 用 IOCP
- **RAII socket 管理** — `Socket` 类自动管理 fd 生命周期
- **应用层缓冲区** — Buffer 三区布局（prependable/readable/writable），支持 `readv` scatter-gather I/O
- **连接生命周期** — `shared_ptr` + `enable_shared_from_this` 管理 TcpConnection
- **线程安全任务投递** — `runInLoop` / `queueInLoop` + socketpair 唤醒

## 依赖

- **编译器**：支持 C++20（GCC 11+ / Clang 14+ / MSVC 2022+）
- **构建工具**：CMake 3.16+
- **系统库**：
  - Linux: `pthread`
  - Windows: `ws2_32`, `mswsock`

无其他第三方依赖。

## 后续规划

- [ ] **IocpPoller** — Windows IOCP 异步 IO 实现
- [ ] **EventLoopThreadPool** — 多线程 Reactor 池
- [ ] **TimerQueue** — 定时器（Linux: `timerfd`, Windows: `CreateWaitableTimer`）
- [ ] **TcpClient + Connector** — 客户端连接器
- [ ] **HTTP 模块** — 简单 HTTP Server/Client
- [ ] **日志系统** — 异步日志

## 参考资料

- [muduo 网络库](https://github.com/chenshuo/muduo) — 陈硕
- [《Linux 多线程服务端编程》](https://book.douban.com/subject/20471211/)
- [muduo 源码分析](https://github.com/chenshuo/muduo)
