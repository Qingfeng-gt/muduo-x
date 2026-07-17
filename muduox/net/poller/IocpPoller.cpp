//
// Created by Administrator on 2026/7/16.
//

#ifdef _WIN32

#include "IocpPoller.h"
#include "muduox/net/core/Channel.h"
#include <cassert>
#include <cstddef>
#include <cstdio>

namespace muduox {

IocpPoller::IocpContext* IocpPoller::contextFromOverlapped(OVERLAPPED* ol,
                                                             bool* outIsRead) {
    // try as readOl
    {
        auto* ctx = reinterpret_cast<IocpContext*>(
            reinterpret_cast<char*>(ol) - offsetof(IocpContext, readOl));
        if (ctx->magic == IocpContext::kMagic && &ctx->readOl == ol) {
            if (outIsRead) *outIsRead = true;
            return ctx;
        }
    }
    // try as writeOl
    {
        auto* ctx = reinterpret_cast<IocpContext*>(
            reinterpret_cast<char*>(ol) - offsetof(IocpContext, writeOl));
        if (ctx->magic == IocpContext::kMagic && &ctx->writeOl == ol) {
            if (outIsRead) *outIsRead = false;
            return ctx;
        }
    }
    return nullptr;
}

IocpPoller::IocpPoller(EventLoop* loop)
    : Poller(loop),
      iocpHandle_(::CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, 0))
{
    assert(iocpHandle_ != nullptr);
}

IocpPoller::~IocpPoller() {
    if (iocpHandle_) {
        ::CloseHandle(iocpHandle_);
    }
    for (auto* ctx : deadContexts_) {
        delete ctx;
    }
}

void IocpPoller::postRead(IocpContext* ctx) {
    if (ctx->readPending) return;

    ctx->readWsa.buf = ctx->readBuf;
    ctx->readWsa.len = 0;  // zero-byte notification
    DWORD flags = 0;
    int ret = ::WSARecv((SOCKET)ctx->channel->fd(),
                         &ctx->readWsa, 1,
                         nullptr, &flags,
                         &ctx->readOl, nullptr);
    if (ret == 0 || ::WSAGetLastError() == WSA_IO_PENDING) {
        ctx->readPending = true;
    }
}

void IocpPoller::postWrite(IocpContext* ctx) {
    if (ctx->writePending) return;

    ctx->writeWsa.buf = ctx->writeBuf;
    ctx->writeWsa.len = 0;  // zero-byte notification
    int ret = ::WSASend((SOCKET)ctx->channel->fd(),
                         &ctx->writeWsa, 1,
                         nullptr, 0,
                         &ctx->writeOl, nullptr);
    if (ret == 0 || ::WSAGetLastError() == WSA_IO_PENDING) {
        ctx->writePending = true;
    }
}

void IocpPoller::updateChannel(Channel* channel) {
    bool isNew = (channel->index() < 0);

    if (isNew) {
        // bind socket to IOCP (only once per socket)
        HANDLE h = ::CreateIoCompletionPort(
            (HANDLE)(SOCKET)channel->fd(),
            iocpHandle_,
            0,
            0);
        if (h == nullptr) {
            DWORD err = ::GetLastError();
            std::fprintf(stderr, "IocpPoller: CreateIoCompletionPort failed for fd=%lld, err=%lu, falling back to single thread\n",
                         (long long)channel->fd(), (unsigned long)err);
            return;
        }

        auto* ctx = new IocpContext;
        ctx->channel = channel;
        channel->context() = ctx;
    }

    channel->setIndex(0);

    auto* ctx = std::any_cast<IocpContext*>(channel->context());
    assert(ctx != nullptr);

    // listening socket can't use WSARecv, fallback to select
    int acceptConn = 0;
    int acceptLen = sizeof(acceptConn);
    ::getsockopt((SOCKET)channel->fd(), SOL_SOCKET, SO_ACCEPTCONN,
                 (char*)&acceptConn, &acceptLen);
    if (acceptConn) {
        selectChannels_.insert(channel);
    } else if (channel->isReading() || channel->isWriting()) {
        pendingPosts_.insert(channel);
    }

    channels_[channel->fd()] = channel;
}

void IocpPoller::removeChannel(Channel* channel) {
    channels_.erase(channel->fd());
    pendingPosts_.erase(channel);
    selectChannels_.erase(channel);

    auto* ctx = std::any_cast<IocpContext*>(channel->context());
    if (ctx) {
        ctx->channel = nullptr;  // mark as removed, poll() will skip
        deadContexts_.push_back(ctx);
        channel->context().reset();
    }

    channel->setIndex(-1);
}

void IocpPoller::poll(int timeoutMs, ChannelList* activeChannels) {
    // Retry pending IO posts. Channels in pendingPosts_ haven't successfully
    // posted their IOCP IO yet (e.g. socket still connecting → WSAENOTCONN).
    if (!pendingPosts_.empty()) {
        auto it = pendingPosts_.begin();
        while (it != pendingPosts_.end()) {
            Channel* channel = *it;
            auto* ctx = std::any_cast<IocpContext*>(channel->context());
            if (ctx == nullptr || ctx->channel == nullptr) {
                it = pendingPosts_.erase(it);
                continue;
            }

            bool done = true;
            if (channel->isReading() && !ctx->readPending) {
                postRead(ctx);
                if (!ctx->readPending) done = false;
            }
            if (channel->isWriting() && !ctx->writePending) {
                postWrite(ctx);
                if (!ctx->writePending) done = false;
            }

            if (done) {
                it = pendingPosts_.erase(it);
            } else {
                ++it;
            }
        }

        if (!pendingPosts_.empty() && timeoutMs > 100) {
            timeoutMs = 100;
        }
    }

    DWORD timeout = (timeoutMs < 0) ? INFINITE : static_cast<DWORD>(timeoutMs);

    std::map<Channel*, int> channelRevents;

    for (int i = 0; i < 128; ++i) {
        DWORD bytesTransferred = 0;
        ULONG_PTR completionKey = 0;
        OVERLAPPED* overlapped = nullptr;

        DWORD t = (i == 0) ? timeout : 0;

        BOOL ok = ::GetQueuedCompletionStatus(
            iocpHandle_,
            &bytesTransferred,
            &completionKey,
            &overlapped,
            t);

        if (overlapped == nullptr) {
            break;
        }

        bool isRead = false;
        IocpContext* ctx = contextFromOverlapped(overlapped, &isRead);
        if (ctx == nullptr) {
            continue;
        }

        Channel* channel = ctx->channel;

        if (channel == nullptr) {
            ctx->readPending = false;
            ctx->writePending = false;
            continue;
        }

        int revents = 0;

        if (isRead) {
            ctx->readPending = false;

            if (!ok) {
                revents |= Channel::kErrorEvent;
            } else if (channel->isReading()) {
                revents |= Channel::kReadEvent;
            }

            // re-post read if channel still wants it, but skip if we already
            // added a read event this round (to avoid 0-byte WSARecv loop)
            if (channel->isReading() && ctx->channel != nullptr &&
                !(revents & Channel::kErrorEvent)) {
                auto it = channelRevents.find(channel);
                bool alreadyHasRead = (it != channelRevents.end() &&
                                       (it->second & Channel::kReadEvent));
                if (!alreadyHasRead) {
                    postRead(ctx);
                }
            }
        } else {
            ctx->writePending = false;

            if (!ok) {
                revents |= Channel::kErrorEvent;
            } else if (channel->isWriting()) {
                revents |= Channel::kWriteEvent;
            }
        }

        if (revents != 0) {
            channelRevents[channel] |= revents;
        }
    }

    for (auto& [ch, rev] : channelRevents) {
        ch->setRevents(rev);
        activeChannels->push_back(ch);
    }

    // select fallback for sockets that can't use IOCP (e.g. listening socket)
    if (!selectChannels_.empty()) {
        fd_set rfds;
        FD_ZERO(&rfds);
        int maxfd = 0;
        for (auto* ch : selectChannels_) {
            SOCKET s = (SOCKET)ch->fd();
            FD_SET(s, &rfds);
            if ((int)s > maxfd) maxfd = (int)s;
        }
        timeval tv{0, 0};
        int n = ::select(maxfd + 1, &rfds, nullptr, nullptr, &tv);
        if (n > 0) {
            for (auto* ch : selectChannels_) {
                if (FD_ISSET((SOCKET)ch->fd(), &rfds) && ch->isReading()) {
                    int rev = Channel::kReadEvent;
                    auto it = channelRevents.find(ch);
                    if (it != channelRevents.end()) {
                        it->second |= rev;
                    } else {
                        ch->setRevents(rev);
                        activeChannels->push_back(ch);
                    }
                }
            }
        }
    }
}

} // namespace muduox

#endif // _WIN32
