# muduox

> 从零实现的跨平台 muduo 风格高性能 C++20 网络库

muduox 是对陈硕 [muduo](https://github.com/chenshuo/muduo) 网络库的复刻与扩展，在保留 Reactor 架构精髓的基础上，**原生支持 Linux（epoll）和 Windows（IOCP）双平台**。

## 架构

```
                           +------------------+
                           |    TcpServer     |     用户层
                           +--------+---------+
                                    |
                           +--------+---------+     +-----------------+
                           |   TcpConnection  |-----|   ThreadPool    |  计算线程池
                           +--------+---------+     +-----------------+
                                    |
                    +---------------+---------------+
                    |                               |
            +-------+--------+             +--------+-----------+
            |   Acceptor      |             | EventLoopThreadPool |  IO线程池
            +-------+--------+             +--------+-----------+
                    |                               |
            +-------+--------+             +--------+-----------+
            |   Socket        |             |  EventLoop (N个)    |  事件循环
            +-------+--------+             +--------+-----------+
                                               /          \
                                              /            \
                                 +--------+------+   +------+--------+
                                 | EpollPoller   |   | IocpPoller    |
                                 | (Linux)       |   | (Windows)     |
                                 +---------------+   +---------------+
```

**三层线程模型** — 主线程 + 1个accept线程 + N个IO线程，配合ThreadPool卸载CPU计算。

## 当前进度

- [x] **阶段一** — 项目骨架 + CMake 跨平台构建
- [x] **阶段二** — Socket 层（SocketOps + Socket + InetAddress）
- [x] **阶段三** — Reactor 核心（Channel + Poller + EpollPoller + EventLoop）
- [x] **阶段四** — 网络层（Acceptor + Buffer + TcpConnection + TcpServer）
- [x] **阶段五** — IOCP Poller（Windows 异步 IO）
- [x] **阶段六** — 扩展（EventLoopThreadPool、ThreadPool、BlockQueue）
- [x] **阶段七** — 日志系统（Logger + AsyncLogging + LogFile + FixedBuffer）
- [ ] 后续 — TimerQueue、TcpClient + Connector、HTTP 模块

## 项目结构

```
muduox/
├── CMakeLists.txt
├── README.md
├── muduox/
│   ├── base/
│   │   ├── platform/
│   │   │   ├── Platform.h              # 跨平台宏抽象（socket、IO、OS 差异）
│   │   │   ├── noncopyable.h
│   │   │   └── copyable.h
│   │   └── logging/
│   │       ├── Logging.h/.cpp           # 同步日志（基于 std::format）
│   │       ├── FixedBuffer.h            # 固定大小无锁缓冲区
│   │       ├── LogFile.h/.cpp           # 文件滚动日志
│   │       └── AsyncLogging.h/.cpp      # 异步日志（后台线程刷盘）
│   └── net/
│       ├── core/
│       │   ├── Callbacks.h              # 回调类型定义
│       │   ├── Channel.h/.cpp           # 事件分发单元（fd + 回调）
│       │   ├── Poller.h/.cpp            # IO 多路复用抽象基类
│       │   ├── EventLoop.h/.cpp         # Reactor 核心（wakeup + 任务队列）
│       │   ├── EventLoopThreadPool.h/.cpp  # IO 线程池（accept线程 + N个worker）
│       │   ├── ThreadPool.h/.cpp        # 计算线程池
│       │   └── BlockQueue.h             # 有界阻塞队列
│       ├── poller/
│       │   ├── EpollPoller.h/.cpp       # Linux epoll 实现
│       │   └── IocpPoller.h/.cpp        # Windows IOCP 实现
│       └── tcp/
│           ├── InetAddress.h/.cpp       # IP 地址 + 端口封装
│           ├── SocketOps.h/.cpp         # 跨平台 socket 操作
│           ├── Socket.h/.cpp            # Socket RAII 封装
│           ├── Acceptor.h/.cpp          # 监听器（accept + Channel）
│           ├── Buffer.h/.cpp            # 应用层缓冲区（三区布局 + readv）
│           ├── TcpConnection.h/.cpp     # TCP 连接（shared_ptr 管理）
│           └── TcpServer.h/.cpp         # TCP 服务器
└── examples/
    └── echo/
        ├── main.cpp                     # Echo Server（端口 8888）
        └── client.cpp                   # Echo Client
```

## 快速开始

### 构建

**Linux：**

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

**macOS：**

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(sysctl -n hw.ncpu)
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

### 运行

```bash
# 终端 1：Echo Server
./build/echo_server

# 终端 2：Echo Client
./build/echo_client
```

### 代码示例

单线程 Echo Server：

```cpp
#include "muduox/net/tcp/TcpServer.h"
#include "muduox/net/tcp/InetAddress.h"
#include "muduox/net/core/EventLoop.h"
#include "muduox/net/tcp/SocketOps.h"

int main() {
    muduox::sockets::startup();
    muduox::EventLoop loop;
    muduox::TcpServer server(muduox::InetAddress(8888), "Echo");

    server.setMessageCallback([](const auto& conn, auto* buf) {
        conn->send(buf->peek(), buf->readableBytes());
        buf->retrieveAll();
    });
    server.start();
    loop.loop();
    muduox::sockets::cleanup();
}
```

多线程（accept线程 + N 个 IO 线程）：

```cpp
// TcpServer 构造函数第三个参数指定 IO 线程数
muduox::TcpServer server(muduox::InetAddress(8888), "Echo", 4);
server.setMessageCallback([](const auto& conn, auto* buf) {
    // 回调在对应连接的 IO 线程中执行
    conn->send(buf->retrieveAllAsString());
});
server.start();
server.getBaseLoop()->loop();  // accept 线程进入事件循环
```

配合计算线程池卸载 CPU 任务：

```cpp
muduox::thread::ThreadPool computePool(4);

server.setMessageCallback([&](const auto& conn, auto* buf) {
    std::string data = buf->retrieveAllAsString();
    computePool.push([conn, data = std::move(data)]() {
        auto result = heavyCompute(data);          // 工作线程
        conn->getLoop()->runInLoop([conn, result]() {
            conn->send(result);                     // 回到 IO 线程发送
        });
    });
});
```

## 核心设计

### one loop per thread

每个 EventLoop 独占一个线程，通过 `thread_local` 确保 "一个线程最多一个 EventLoop"。Channel 绑定到 EventLoop，所有 IO 操作都在所属线程内完成，无需锁。

### 跨平台 Poller

| 特性 | EpollPoller (Linux) | IocpPoller (Windows) |
|------|---------------------|----------------------|
| 模式 | Reactor：epoll_wait → 回调 | Proactor→Reactor 适配：IOCP 完成通知 → 事件翻译 |
| accept | `accept4(SOCK_NONBLOCK)` | `WSASocketW` + `CreateIoCompletionPort` |
| 事件通知 | `epoll_wait` | `GetQueuedCompletionStatus` |
| 监听 socket | epoll 直接支持 | select fallback |

IOCP 下通过零字节 `WSARecv`/`WSASend` 预投递异步 IO，完成时翻译为 `kReadEvent`/`kWriteEvent`，保持上层接口一致。

### 线程同步

- **IO 线程间**：无竞争，每个连接绑定一个 IO 线程
- **跨线程任务**：`runInLoop` / `queueInLoop` + socketpair wakeup
- **计算卸载**：`ThreadPool`（BlockQueue + mutex + condition_variable）
- **连接状态**：`std::atomic<State>` 保证多线程读写安全

## 技术特性

- **C++20** — 使用 `std::format`、`std::source_location`、`std::atomic`、`std::any` 等
- **Reactor 模式** — 非阻塞 IO + IO 多路复用
- **跨平台** — `#ifdef _WIN32` + `Platform.h` 宏抽象层
- **RAII** — Socket/Channel/EventLoop 自动管理资源生命周期
- **应用层缓冲区** — prependable/readable/writable 三区布局，`readv` scatter-gather I/O
- **连接生命周期** — `shared_ptr` + `enable_shared_from_this`，防止悬挂引用
- **日志** — 基于 `std::format`，支持异步日志 + 文件滚动

## 依赖

- **编译器**：C++20（GCC 11+ / Clang 14+ / MSVC 2022+）
- **构建工具**：CMake 3.16+
- **系统库**：Linux `pthread`、Windows `ws2_32` `mswsock`

无第三方依赖。

## 后续规划

- [ ] **TimerQueue** — 定时器（Linux: `timerfd`, Windows: `CreateWaitableTimer`）
- [ ] **TcpClient + Connector** — 客户端连接器
- [ ] **HTTP 模块** — 简单 HTTP Server/Client

## 参考资料

- [muduo 网络库](https://github.com/chenshuo/muduo) — 陈硕
- [《Linux 多线程服务端编程》](https://book.douban.com/subject/20471211/)
