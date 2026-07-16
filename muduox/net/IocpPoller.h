//
// Created by Administrator on 2026/7/16.
//

#ifndef MUDUOX_IOCPPOLLER_H
#define MUDUOX_IOCPPOLLER_H

#ifdef _WIN32

#include "Poller.h"
#include <map>
#include <set>
#include <winsock2.h>
#include <windows.h>

namespace muduox {

///
/// Windows IOCP（I/O Completion Port）Poller 实现。
///
/// IOCP 本质是 Proactor 模式（投递异步 IO → 完成回调），
/// 这里通过 **零字节 WSARecv/WSASend** 的技巧将其适配为 Reactor 风格：
///
///   1. enableReading() → 投递零字节 WSARecv 作为"可读通知"
///   2. enableWriting() → 投递零字节 WSASend 作为"可写通知"
///   3. poll() → GetQueuedCompletionStatus 等待完成
///   4. 完成时翻译为 kReadEvent / kWriteEvent / kCloseEvent / kErrorEvent
///   5. 上层 TcpConnection / Acceptor 仍使用同步 ::recv / ::send / ::accept
///
class IocpPoller : public Poller {
public:
    explicit IocpPoller(EventLoop* loop);
    ~IocpPoller() override;

    void poll(int timeoutMs, ChannelList* activeChannels) override;
    void updateChannel(Channel* channel) override;
    void removeChannel(Channel* channel) override;

private:
    ///
    /// 每个 socket 关联的 IOCP 上下文。
    /// 包含独立的读/写 OVERLAPPED，用于零字节通知。
    ///
    struct IocpContext {
        static constexpr uint32_t kMagic = 0x494F4350; // "IOCP" in ASCII, for validation

        uint32_t   magic = kMagic;

        // ---- 读通知 ----
        OVERLAPPED readOl{};
        char       readBuf[1]{};   // 零字节 recv 仍需一个缓冲区
        WSABUF     readWsa{1, readBuf};
        bool       readPending = false;

        // ---- 写通知 ----
        OVERLAPPED writeOl{};
        char       writeBuf[1]{};
        WSABUF     writeWsa{1, writeBuf};
        bool       writePending = false;

        Channel*   channel = nullptr;
    };

    void postRead(IocpContext* ctx);
    void postWrite(IocpContext* ctx);

    static IocpContext* contextFromOverlapped(OVERLAPPED* ol, bool* outIsRead);

    HANDLE iocpHandle_;
    std::map<intptr_t, Channel*> channels_;
    std::vector<IocpContext*> deadContexts_;  // 待清理的无效 context
    std::set<Channel*> pendingPosts_;         // 延迟投递 IO 的 channel 集合
    std::set<Channel*> selectChannels_;       // 不能用 IOCP（如监听 socket），用 select 回退
};

} // namespace muduox

#endif // _WIN32
#endif // MUDUOX_IOCPPOLLER_H
