//
// Created by Administrator on 2026/7/16.
//

#ifndef MUDUOX_IOCPPOLLER_H
#define MUDUOX_IOCPPOLLER_H

#ifdef _WIN32

#include "muduox/net/core/Poller.h"
#include <map>
#include <set>
#include <winsock2.h>
#include <windows.h>

namespace muduox {

// Windows IOCP poller: adapts Proactor-style IOCP to Reactor-style via
// zero-byte WSARecv/WSASend as readiness notifications.
class IocpPoller : public Poller {
public:
    explicit IocpPoller(EventLoop* loop);
    ~IocpPoller() override;

    void poll(int timeoutMs, ChannelList* activeChannels) override;
    void updateChannel(Channel* channel) override;
    void removeChannel(Channel* channel) override;

private:
    // Per-socket IOCP context with independent read/write OVERLAPPED.
    struct IocpContext {
        static constexpr uint32_t kMagic = 0x494F4350;

        uint32_t   magic = kMagic;

        OVERLAPPED readOl{};
        char       readBuf[1]{};
        WSABUF     readWsa{1, readBuf};
        bool       readPending = false;

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
    std::vector<IocpContext*> deadContexts_;
    std::set<Channel*> pendingPosts_;
    std::set<Channel*> selectChannels_;
};

} // namespace muduox

#endif // _WIN32
#endif // MUDUOX_IOCPPOLLER_H
