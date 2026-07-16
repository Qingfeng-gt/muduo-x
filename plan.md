# muduo 网络库复刻 — C++20 + epoll/IOCP 实现计划

> **For Hermes:** 按阶段逐任务执行，每完成一个阶段 commit 一次。

**Goal:** 从零实现 muduo 风格的高性能跨平台网络库，Linux 用 epoll，Windows 用 IOCP。

**Architecture:** Reactor 风格统一接口，底层 Poller 适配不同平台 —
- Linux: `EpollPoller` (epoll_create/epoll_ctl/epoll_wait)
- Windows: `IocpPoller` (CreateIoCompletionPort/GetQueuedCompletionStatus，Proactor 适配为 Reactor)

**Tech Stack:** C++20, CMake 3.16+, epoll (Linux), IOCP (Windows), 无第三方依赖

---

## 关键设计决策

### epoll vs IOCP 的差异

| 特性 | epoll (Linux) | IOCP (Windows) |
|------|---------------|----------------|
| 模式 | Reactor：等事件 → 自己 read/write | Proactor：投递异步 IO → 完成通知 |
| accept | `accept4()` 直接拿到 fd | `AcceptEx` 异步接受，需预创建 socket |
| recv | `read()` / `recv()` 同步读 | `WSARecv` 异步读，需预投递 |
| send | `write()` / `send()` 同步写 | `WSASend` 异步写 |
| 事件模型 | EPOLLIN / EPOLLOUT / EPOLLERR | completion key + overlapped |
| fd 注册 | `epoll_ctl(ADD/MOD/DEL)` | `CreateIoCompletionPort` 绑定 |

### 统一方案

Poller 接口保持 Reactor 风格，IocpPoller 内部做 Proactor→Reactor 的适配：

1. **IocpContext** 绑定每个 socket，持有预分配的 `OVERLAPPED` + `WSABUF`
2. `enableReading()` → 投递一个 `WSARecv` 到 IOCP
3. `enableWriting()` → 投递一个 `WSASend`（按需，通常由用户触发）
4. `poll()` → `GetQueuedCompletionStatus` 等待完成事件，翻译为 `readEvent`/`writeEvent`
5. `accept` → 用 `AcceptEx` 预投递，新连接到达时完成

---

## 项目结构

```
D:\Hermes home\muduo-cross\
├── CMakeLists.txt
├── muduo/
│   ├── base/
│   │   ├── noncopyable.h
│   │   └── Timestamp.h / .cc
│   └── net/
│       ├── Callbacks.h
│       ├── InetAddress.h / .cc
│       ├── Socket.h / .cc          # 跨平台 socket RAII
│       ├── Channel.h / .cc         # fd + 回调（epoll）/ ctx + 回调（IOCP）
│       ├── Poller.h                # 抽象基类
│       ├── EpollPoller.h / .cc     # Linux epoll
│       ├── IocpPoller.h / .cc      # Windows IOCP + IocpContext
│       ├── EventLoop.h / .cc       # 事件循环
│       ├── Acceptor.h / .cc
│       ├── TcpConnection.h / .cc
│       ├── TcpServer.h / .cc
│       ├── Buffer.h / .cc
│       └── SocketOps.h / .cc       # 跨平台 socket 操作辅助
├── examples/
│   └── echo/
│       └── main.cc
└── tests/                          # 可选
```

---

## 阶段一：项目骨架 + 跨平台基础设施

### Task 1.1: CMakeLists.txt + C++20 + 平台检测

```cmake
cmake_minimum_required(VERSION 3.16)
project(muduo-cross VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 平台检测
if(WIN32)
    set(MUDUO_PLATFORM "windows")
    set(PLATFORM_LIBS ws2_32 mswsock)
    set(MUDUO_POLLER_SRC muduo/net/IocpPoller.cc)
elseif(APPLE)
    set(MUDUO_PLATFORM "macos")
    set(PLATFORM_LIBS pthread)
    set(MUDUO_POLLER_SRC muduo/net/EpollPoller.cc)
else()
    set(MUDUO_PLATFORM "linux")
    set(PLATFORM_LIBS pthread)
    set(MUDUO_POLLER_SRC muduo/net/EpollPoller.cc)
endif()

message(STATUS "muduo-cross platform: ${MUDUO_PLATFORM}")

add_library(muduo_net STATIC
    muduo/base/Timestamp.cc
    muduo/net/InetAddress.cc
    muduo/net/Socket.cc
    muduo/net/SocketOps.cc
    muduo/net/Channel.cc
    muduo/net/EventLoop.cc
    muduo/net/Acceptor.cc
    muduo/net/TcpConnection.cc
    muduo/net/TcpServer.cc
    muduo/net/Buffer.cc
    ${MUDUO_POLLER_SRC}
)

target_include_directories(muduo_net PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(muduo_net PUBLIC ${PLATFORM_LIBS})

add_executable(echo_server examples/echo/main.cc)
target_link_libraries(echo_server muduo_net)
```

### Task 1.2: noncopyable + Timestamp (C++20 改进)

与之前计划相同，增加 C++20 `std::chrono` 的更好用法，`Timestamp::now()` 可直接用 `system_clock`。

---

## 阶段二：Socket 层（跨平台封装）

### Task 2.1: SocketOps — 跨平台 socket 操作

**文件:** `muduo/net/SocketOps.h`, `muduo/net/SocketOps.cc`

封装所有平台差异：

```cpp
// SocketOps.h
#pragma once
#include <cstdint>

namespace muduo::sockets {

// ---- 初始化 / 清理 (Windows 需要 WSAStartup) ----
void startup();
void cleanup();

// ---- 创建 ----
int createTcpSocket();         // socket() + nonblock
int createTcpSocketPair(int fds[2]); // wakeup 用

// ---- 操作 ----
void bind(int fd, uint32_t ip, uint16_t port);
void listen(int fd);
int  accept(int fd, uint32_t* ip, uint16_t* port);
void close(int fd);
void shutdownWrite(int fd);

void setNonBlocking(int fd);
void setReuseAddr(int fd, bool on);
void setTcpNoDelay(int fd, bool on);
void setKeepAlive(int fd, bool on);

// ---- 地址 ----
uint32_t hostToNetwork32(uint32_t host);
uint16_t hostToNetwork16(uint16_t host);
uint32_t networkToHost32(uint32_t net);
uint16_t networkToHost16(uint16_t net);
void     toIpPort(char* buf, size_t size, uint32_t ip, uint16_t port);

} // namespace muduo::sockets
```

实现中 `#ifdef _WIN32` 处理所有差异。

### Task 2.2: InetAddress + Socket

与之前计划相同，但内部调用 `sockets::*` 辅助函数。

---

## 阶段三：Reactor 核心 — Channel + Poller + EventLoop

### Task 3.1: Channel（事件分发单元）

**关键变化：**
- epoll 下：绑定 fd + 事件掩码，由 EpollPoller 管理
- IOCP 下：绑定 IocpContext（持有 OVERLAPPED + WSABUF），由 IocpPoller 管理

```cpp
// Channel.h
#pragma once
#include "muduo/base/noncopyable.h"
#include <functional>
#include <memory>
#include <any>  // C++17; C++20 也可以用

namespace muduo {

class EventLoop;

class Channel : noncopyable {
public:
    static constexpr int kNoneEvent  = 0;
    static constexpr int kReadEvent  = 1 << 0;
    static constexpr int kWriteEvent = 1 << 1;
    static constexpr int kErrorEvent = 1 << 2;
    static constexpr int kCloseEvent = 1 << 3;

    using EventCallback = std::function<void()>;

    Channel(EventLoop* loop, int fd);
    ~Channel();

    int  fd() const { return fd_; }
    int  events() const { return events_; }
    void setRevents(int revt) { revents_ = revt; }

    void enableReading();
    void disableReading();
    void enableWriting();
    void disableWriting();
    void disableAll();

    bool isReading() const { return events_ & kReadEvent; }
    bool isWriting() const { return events_ & kWriteEvent; }

    // 回调设置
    void setReadCallback(EventCallback cb)  { readCallback_ = std::move(cb); }
    void setWriteCallback(EventCallback cb) { writeCallback_ = std::move(cb); }
    void setCloseCallback(EventCallback cb) { closeCallback_ = std::move(cb); }
    void setErrorCallback(EventCallback cb) { errorCallback_ = std::move(cb); }

    void handleEvent();        // 根据 revents_ 分发回调
    void handleEvent(int revents); // IOCP 版本：直接传入事件

    // Poller 管理用
    int  index() const { return index_; }
    void setIndex(int idx) { index_ = idx; }
    EventLoop* ownerLoop() const { return loop_; }

    void remove();

    // ---- IOCP 专用接口 ----
    // 在 Windows 上，Channel 需要关联 IocpContext 来管理异步 IO
    // 通过 std::any 存储平台相关数据，避免暴露 IOCP 头文件
    std::any& context() { return context_; }
    const std::any& context() const { return context_; }

private:
    void update();

    EventLoop* loop_;
    const int fd_;
    int events_  = kNoneEvent;
    int revents_ = kNoneEvent;
    int index_   = -1;            // Poller 内部索引用

    std::any context_;            // IocpContext* (仅 Windows)

    EventCallback readCallback_;
    EventCallback writeCallback_;
    EventCallback closeCallback_;
    EventCallback errorCallback_;
};

} // namespace muduo
```

### Task 3.2: Poller 抽象基类

```cpp
// Poller.h
#pragma once
#include "muduo/base/noncopyable.h"
#include <vector>
#include <memory>

namespace muduo {

class Channel;
class EventLoop;

class Poller : noncopyable {
public:
    using ChannelList = std::vector<Channel*>;

    explicit Poller(EventLoop* loop) : ownerLoop_(loop) {}
    virtual ~Poller() = default;

    virtual void poll(int timeoutMs, ChannelList* activeChannels) = 0;
    virtual void updateChannel(Channel* channel) = 0;
    virtual void removeChannel(Channel* channel) = 0;

    static std::unique_ptr<Poller> create(EventLoop* loop);

protected:
    EventLoop* ownerLoop_;
};

} // namespace muduo
```

### Task 3.3: EpollPoller（Linux/macOS）

```cpp
// EpollPoller.h
#pragma once
#include "muduo/net/Poller.h"
#include <map>

namespace muduo {

class EpollPoller : public Poller {
public:
    explicit EpollPoller(EventLoop* loop);
    ~EpollPoller() override;

    void poll(int timeoutMs, ChannelList* activeChannels) override;
    void updateChannel(Channel* channel) override;
    void removeChannel(Channel* channel) override;

private:
    void update(int operation, Channel* channel);

    static constexpr int kInitEventListSize = 16;

    int epollFd_;
    std::vector<struct epoll_event> events_;
    std::map<int, Channel*> channels_;
};

} // namespace muduo
```

```cpp
// EpollPoller.cc
#include "muduo/net/EpollPoller.h"
#include "muduo/net/Channel.h"
#include <sys/epoll.h>
#include <unistd.h>

namespace muduo {

EpollPoller::EpollPoller(EventLoop* loop)
    : Poller(loop),
      epollFd_(::epoll_create1(EPOLL_CLOEXEC)),
      events_(kInitEventListSize) {}

EpollPoller::~EpollPoller() { ::close(epollFd_); }

void EpollPoller::poll(int timeoutMs, ChannelList* activeChannels) {
    int numEvents = ::epoll_wait(epollFd_, events_.data(),
                                  static_cast<int>(events_.size()), timeoutMs);
    if (numEvents > 0) {
        for (int i = 0; i < numEvents; ++i) {
            Channel* channel = static_cast<Channel*>(events_[i].data.ptr);
            int revents = 0;
            if (events_[i].events & (EPOLLIN | EPOLLPRI | EPOLLRDHUP))
                revents |= Channel::kReadEvent;
            if (events_[i].events & EPOLLOUT)
                revents |= Channel::kWriteEvent;
            if (events_[i].events & EPOLLERR)
                revents |= Channel::kErrorEvent;
            if (events_[i].events & EPOLLHUP)
                revents |= Channel::kCloseEvent;
            channel->setRevents(revents);
            activeChannels->push_back(channel);
        }
        if (numEvents == static_cast<int>(events_.size())) {
            events_.resize(events_.size() * 2);
        }
    }
}

void EpollPoller::updateChannel(Channel* channel) {
    int op = (channel->index() < 0) ? EPOLL_CTL_ADD : EPOLL_CTL_MOD;
    update(op, channel);
}

void EpollPoller::removeChannel(Channel* channel) {
    update(EPOLL_CTL_DEL, channel);
    channels_.erase(channel->fd());
}

void EpollPoller::update(int operation, Channel* channel) {
    epoll_event event{};
    event.events = 0;
    if (channel->isReading()) event.events |= (EPOLLIN | EPOLLPRI);
    if (channel->isWriting()) event.events |= EPOLLOUT;
    event.data.ptr = channel;
    ::epoll_ctl(epollFd_, operation, channel->fd(), &event);
    channel->setIndex(operation == EPOLL_CTL_DEL ? -1 : 0);
    if (operation != EPOLL_CTL_DEL) {
        channels_[channel->fd()] = channel;
    }
}

} // namespace muduo
```

### Task 3.4: IocpPoller（Windows）

这是最关键也是最复杂的部分。

**设计：**

```
IocpPoller
  ├── iocpHandle_ (HANDLE)
  ├── channels_ (map<int, Channel*>)
  │
  └── 每个 Channel 关联 IocpContext:
        ├── OVERLAPPED (异步 IO 控制块)
        ├── WSABUF     (IO 缓冲区)
        ├── operation  (READ / WRITE / ACCEPT)
        └── Channel*   (回指)
```

**IocpContext** — 每次异步 IO 的状态：

```cpp
// IocpPoller.h (内部)
struct IocpContext {
    OVERLAPPED overlapped{};
    WSABUF     wsabuf{};
    DWORD      bytesTransferred = 0;
    DWORD      flags = 0;

    enum Op { kNone, kRead, kWrite, kAccept };
    Op op = kNone;

    // 缓冲区
    std::vector<char> buffer;

    Channel* channel = nullptr;

    void reset() {
        std::memset(&overlapped, 0, sizeof(overlapped));
        op = kNone;
        channel = nullptr;
    }
};
```

**IocpPoller 核心逻辑：**

```cpp
// IocpPoller.h
#pragma once
#include "muduo/net/Poller.h"
#include <map>
#include <windows.h>

namespace muduo {

class IocpPoller : public Poller {
public:
    explicit IocpPoller(EventLoop* loop);
    ~IocpPoller() override;

    void poll(int timeoutMs, ChannelList* activeChannels) override;
    void updateChannel(Channel* channel) override;
    void removeChannel(Channel* channel) override;

    // IocpContext 管理
    struct IocpContext;  // 前向声明
    IocpContext* createContext(Channel* channel);
    void freeContext(IocpContext* ctx);

    // 异步 IO 操作
    bool postRead(Channel* channel);
    bool postWrite(Channel* channel, const char* data, int len);
    bool postAccept(Channel* listenChannel, SOCKET acceptSocket);

private:
    HANDLE iocpHandle_;
    std::map<int, Channel*> channels_;
};

} // namespace muduo
```

**完整实现要点：**

```cpp
// IocpPoller.cc
#include "muduo/net/IocpPoller.h"
#include "muduo/net/Channel.h"
#include <mswsock.h> // AcceptEx

namespace muduo {

// ========== IocpContext 定义 ==========
struct IocpPoller::IocpContext {
    OVERLAPPED overlapped{};
    WSABUF     wsabuf{};
    std::vector<char> buffer;
    enum Op { kNone, kRead, kWrite, kAccept };
    Op op = kNone;
    Channel* channel = nullptr;
    SOCKET acceptSocket = INVALID_SOCKET; // AcceptEx 用

    void reset() {
        ZeroMemory(&overlapped, sizeof(overlapped));
        op = kNone;
    }
};

IocpPoller::IocpPoller(EventLoop* loop)
    : Poller(loop),
      iocpHandle_(CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, 0))
{
    if (!iocpHandle_) {
        // error
    }
}

IocpPoller::~IocpPoller() {
    if (iocpHandle_) CloseHandle(iocpHandle_);
}

void IocpPoller::poll(int timeoutMs, ChannelList* activeChannels) {
    DWORD timeout = (timeoutMs < 0) ? INFINITE : static_cast<DWORD>(timeoutMs);

    // 一次可能完成多个 IO
    for (int i = 0; i < 128; ++i) {
        DWORD bytesTransferred = 0;
        ULONG_PTR completionKey = 0;
        OVERLAPPED* overlapped = nullptr;

        // 第一次用超时，后续用 0（非阻塞轮询）
        DWORD t = (i == 0) ? timeout : 0;
        BOOL ok = GetQueuedCompletionStatus(iocpHandle_, &bytesTransferred,
                                             &completionKey, &overlapped, t);
        if (!overlapped) break; // 超时或错误

        // 从 OVERLAPPED 找回 IocpContext
        IocpContext* ctx = CONTAINING_RECORD(overlapped, IocpContext, overlapped);
        Channel* channel = ctx->channel;

        int revents = 0;
        if (ctx->op == IocpContext::kRead && bytesTransferred > 0) {
            revents |= Channel::kReadEvent;
            // 数据在 ctx->buffer 中，需要传给 Channel
        } else if (ctx->op == IocpContext::kRead && bytesTransferred == 0) {
            revents |= Channel::kCloseEvent;
        } else if (ctx->op == IocpContext::kWrite) {
            revents |= Channel::kWriteEvent;
        } else if (ctx->op == IocpContext::kAccept) {
            revents |= Channel::kReadEvent; // accept 完成，有新的连接
        }

        if (revents && channel) {
            channel->setRevents(revents);
            activeChannels->push_back(channel);
        }
    }
}

void IocpPoller::updateChannel(Channel* channel) {
    // 将 socket 绑定到 IOCP
    HANDLE h = CreateIoCompletionPort(
        reinterpret_cast<HANDLE>(channel->fd()),
        iocpHandle_,
        reinterpret_cast<ULONG_PTR>(channel), // completion key = channel
        0);
    (void)h;

    // 创建 IocpContext
    auto* ctx = createContext(channel);
    channel->context() = ctx;
    channels_[channel->fd()] = channel;

    // 如果启用了读，投递第一个 read
    if (channel->isReading()) {
        postRead(channel);
    }
}

void IocpPoller::removeChannel(Channel* channel) {
    channels_.erase(channel->fd());
    auto* ctx = std::any_cast<IocpContext*>(channel->context());
    if (ctx) freeContext(ctx);
    channel->context().reset();
}

IocpPoller::IocpContext* IocpPoller::createContext(Channel* channel) {
    auto* ctx = new IocpContext;
    ctx->channel = channel;
    ctx->buffer.resize(65536); // 64KB read buffer
    ctx->wsabuf.buf = ctx->buffer.data();
    ctx->wsabuf.len = static_cast<ULONG>(ctx->buffer.size());
    return ctx;
}

void IocpPoller::freeContext(IocpContext* ctx) {
    delete ctx;
}

bool IocpPoller::postRead(Channel* channel) {
    auto* ctx = std::any_cast<IocpContext*>(channel->context());
    if (!ctx) return false;

    ctx->reset();
    ctx->op = IocpContext::kRead;
    ctx->wsabuf.buf = ctx->buffer.data();
    ctx->wsabuf.len = static_cast<ULONG>(ctx->buffer.size());

    DWORD flags = 0;
    int ret = WSARecv(channel->fd(), &ctx->wsabuf, 1,
                       nullptr, &flags, &ctx->overlapped, nullptr);
    return (ret == 0 || WSAGetLastError() == WSA_IO_PENDING);
}

bool IocpPoller::postWrite(Channel* channel, const char* data, int len) {
    auto* ctx = std::any_cast<IocpContext*>(channel->context());
    if (!ctx) return false;

    ctx->reset();
    ctx->op = IocpContext::kWrite;
    // 复制数据到 context buffer（简化版）
    ctx->buffer.assign(data, data + len);
    ctx->wsabuf.buf = ctx->buffer.data();
    ctx->wsabuf.len = len;

    int ret = WSASend(channel->fd(), &ctx->wsabuf, 1,
                       nullptr, 0, &ctx->overlapped, nullptr);
    return (ret == 0 || WSAGetLastError() == WSA_IO_PENDING);
}

} // namespace muduo
```

### Task 3.5: EventLoop（C++20 改进）

与之前计划基本相同，`std::any` 替代原始指针存 Poller 上下文，

C++20 改进点：
- 使用 `std::jthread` 替代 `std::thread`（自动 join）
- wakeup 机制：Linux `eventfd`，Windows `socketpair`

---

## 阶段四：网络层

### Task 4.1: Acceptor（epoll 版）

与之前计划相同。

### Task 4.2: Acceptor（IOCP 版适配）

IOCP 下的 accept 使用 `AcceptEx`：

```cpp
// Acceptor 需要预创建 accept socket
// 在 handleRead 中（实际是 IOCP 完成通知），调用 setsockopt 更新 socket
// 然后触发 newConnectionCallback
```

详见 `SocketOps` 中的 `acceptEx` 封装。

### Task 4.3: Buffer + TcpConnection + TcpServer

基本与之前计划相同，但 IOCP 下：
- `TcpConnection::handleRead()` 读取数据来自 IocpContext 的 buffer（不是 `read()`）
- 通过 IocpPoller 的 `postRead/postWrite` 来触发异步 IO

**关键适配：** Channel 的回调可能需要携带额外数据（已读取的字节），或者 Buffer 直接从 IocpContext 中取数据。

---

## 阶段五：示例 + 编译验证

### Task 5.1: 跨平台 socket 初始化

每个平台需要不同的初始化：

```cpp
// Linux: 无需特殊初始化（可能有信号处理 SIGPIPE）
// macOS: 无需
// Windows: WSAStartup + WSACleanup

// 建议用 RAII 包装
class NetworkInitializer {
public:
    NetworkInitializer() {
#ifdef _WIN32
        WSADATA wsa;
        WSAStartup(MAKEWORD(2, 2), &wsa);
#else
        signal(SIGPIPE, SIG_IGN); // 忽略 SIGPIPE
#endif
    }
    ~NetworkInitializer() {
#ifdef _WIN32
        WSACleanup();
#endif
    }
};
```

### Task 5.2: Echo Server

```cpp
#include "muduo/net/EventLoop.h"
#include "muduo/net/TcpServer.h"
#include "muduo/net/InetAddress.h"
#include <iostream>
#include <cstdlib>

int main() {
    muduo::sockets::startup();

    muduo::EventLoop loop;
    muduo::InetAddress listenAddr(8888);
    muduo::TcpServer server(&loop, listenAddr, "EchoServer");

    server.setConnectionCallback([](const muduo::TcpConnectionPtr& conn) {
        std::cout << "[" << conn->peerAddress().toIpPort() << "] connection "
                  << (conn->connected() ? "UP" : "DOWN") << std::endl;
    });

    server.setMessageCallback([](const muduo::TcpConnectionPtr& conn,
                                  std::span<const char> data) {
        conn->send(data.data(), data.size());
    });

    server.start();
    std::cout << "Echo server listening on 0.0.0.0:8888" << std::endl;
    loop.loop();

    muduo::sockets::cleanup();
    return 0;
}
```

### Task 5.3: 编译验证

```powershell
# Windows
cd "D:\Hermes home\muduo-cross"
mkdir build -Force; cd build
cmake .. -G "Visual Studio 17 2022"
cmake --build . --config Release

# Linux
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

---

## 阶段六：扩展（后续可做）

- **TimerQueue** — 定时器，epoll 可用 `timerfd`，IOCP 用 `CreateWaitableTimer`
- **EventLoopThreadPool** — one loop per thread
- **TcpClient + Connector** — 客户端连接
- **HTTP 模块** — 简单的 HTTP server
- **日志系统** — AsyncLogging

---

## 风险矩阵

| 风险 | 严重程度 | 缓解 |
|------|---------|------|
| IOCP Proactor 与 Reactor 接口不匹配 | 高 | 通过 `IocpContext` 预投递 IO 来模拟 Reactor，Channel 接口保持不变 |
| AcceptEx 需要预创建 socket | 中 | Acceptor 内部管理 socket 池 |
| IOCP 下写操作的生命周期 | 中 | `IocpContext` 持有数据副本，写完成后释放 |
| epoll/kqueue macOS 兼容性 | 低 | macOS 也支持 epoll（通过 kqueue 模拟不够好），或单用 kqueue |
| C++20 coroutine 过早引入 | 低 | 先不用协程，保持回调模型简单 |

---

## 文件清单（总计）

| 文件 | 行数（估） | 说明 |
|------|----------|------|
| CMakeLists.txt | ~50 | 构建配置 |
| muduo/base/noncopyable.h | ~15 | 禁止拷贝 |
| muduo/base/Timestamp.h/.cc | ~60 | 时间戳 |
| muduo/net/Callbacks.h | ~30 | 回调类型 |
| muduo/net/SocketOps.h/.cc | ~200 | 跨平台 socket 操作 |
| muduo/net/InetAddress.h/.cc | ~60 | IP 地址 |
| muduo/net/Socket.h/.cc | ~80 | Socket RAII |
| muduo/net/Channel.h/.cc | ~100 | 事件分发 |
| muduo/net/Poller.h | ~30 | 抽象基类 |
| muduo/net/EpollPoller.h/.cc | ~120 | Linux epoll |
| muduo/net/IocpPoller.h/.cc | ~300 | Windows IOCP（最复杂） |
| muduo/net/EventLoop.h/.cc | ~150 | 事件循环 |
| muduo/net/Acceptor.h/.cc | ~80 | 监听器 |
| muduo/net/Buffer.h/.cc | ~120 | 缓冲区 |
| muduo/net/TcpConnection.h/.cc | ~200 | TCP 连接 |
| muduo/net/TcpServer.h/.cc | ~100 | TCP 服务器 |
| examples/echo/main.cc | ~40 | Echo 示例 |
| **总计** | **~1700** | |

---

## 总结

计划已更新为 **C++20 + epoll/IOCP** 方案。核心挑战在于 **IOCP 的 Proactor 模式与 muduo Reactor 接口的适配**，通过 `IocpContext`（预投递异步 IO + 完成时翻译为 Reactor 事件）来解决。

准备好后说 **"开始实现"**，我会从阶段一开始逐步执行。
