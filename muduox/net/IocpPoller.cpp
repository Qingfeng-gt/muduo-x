//
// Created by Administrator on 2026/7/16.
//

#ifdef _WIN32

#include "IocpPoller.h"
#include "Channel.h"
#include <cassert>
#include <cstddef>
#include <cstdio>

namespace muduox {

// ──── 从 OVERLAPPED 还原 IocpContext ─────────────────
// 分别按 readOl / writeOl 偏移尝试，成功则返回 context 指针

IocpPoller::IocpContext* IocpPoller::contextFromOverlapped(OVERLAPPED* ol,
                                                             bool* outIsRead) {
    // 用 magic number 验证正确性，然后通过 offsetof 区分 readOl / writeOl
    // readOl 在 magic 之后（偏移 sizeof(magic)），writeOl 在更后面

    // 按 readOl 还原
    {
        auto* ctx = reinterpret_cast<IocpContext*>(
            reinterpret_cast<char*>(ol) - offsetof(IocpContext, readOl));
        if (ctx->magic == IocpContext::kMagic && &ctx->readOl == ol) {
            if (outIsRead) *outIsRead = true;
            return ctx;
        }
    }
    // 按 writeOl 还原
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

// ──── 构造 / 析构 ────────────────────────────────────

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
    // 清理所有未释放的 context
    for (auto* ctx : deadContexts_) {
        delete ctx;
    }
}

// ──── 投递零字节读通知 ──────────────────────────────

void IocpPoller::postRead(IocpContext* ctx) {
    if (ctx->readPending) return;

    ctx->readWsa.buf = ctx->readBuf;
    ctx->readWsa.len = 0;  // 零字节通知：不消费数据，仅检测可读
    DWORD flags = 0;
    int ret = ::WSARecv((SOCKET)ctx->channel->fd(),
                         &ctx->readWsa, 1,
                         nullptr, &flags,
                         &ctx->readOl, nullptr);
    if (ret == 0 || ::WSAGetLastError() == WSA_IO_PENDING) {
        ctx->readPending = true;
    }
}

// ──── 投递零字节写通知 ──────────────────────────────

void IocpPoller::postWrite(IocpContext* ctx) {
    if (ctx->writePending) return;

    ctx->writeWsa.buf = ctx->writeBuf;
    ctx->writeWsa.len = 0;  // 零字节通知：不发送数据，仅检测可写
    int ret = ::WSASend((SOCKET)ctx->channel->fd(),
                         &ctx->writeWsa, 1,
                         nullptr, 0,
                         &ctx->writeOl, nullptr);
    if (ret == 0 || ::WSAGetLastError() == WSA_IO_PENDING) {
        ctx->writePending = true;
    }
}

// ──── 注册 / 更新 channel ─────────────────────────────

void IocpPoller::updateChannel(Channel* channel) {
    bool isNew = (channel->index() < 0);

    if (isNew) {
        // 将 socket 绑定到 IOCP（仅在首次注册时执行）
        // 注意：对已关联 IOCP 的 socket 重复调用 CreateIoCompletionPort
        //       可能返回 ERROR_INVALID_PARAMETER (87)，因此必须限制为仅首次调用。
        HANDLE h = ::CreateIoCompletionPort(
            (HANDLE)(SOCKET)channel->fd(),
            iocpHandle_,
            0,
            0);
        if (h == nullptr) {
            DWORD err = ::GetLastError();
            std::fprintf(stderr, "CreateIoCompletionPort failed for fd=%lld (raw=0x%llx), iocp=%p, err=%lu\n",
                         (long long)channel->fd(), (unsigned long long)channel->fd(),
                         iocpHandle_, (unsigned long)err);
            assert(false);
        }

        // 创建 IocpContext
        auto* ctx = new IocpContext;
        ctx->channel = channel;
        channel->context() = ctx;
    }

    channel->setIndex(0);

    auto* ctx = std::any_cast<IocpContext*>(channel->context());
    assert(ctx != nullptr);

    // 检测是否为监听 socket：监听 socket 不能用 WSARecv，需要用 select 回退
    int acceptConn = 0;
    int acceptLen = sizeof(acceptConn);
    ::getsockopt((SOCKET)channel->fd(), SOL_SOCKET, SO_ACCEPTCONN,
                 (char*)&acceptConn, &acceptLen);
    if (acceptConn) {
        // 监听 socket → 加入 select 回退集合
        selectChannels_.insert(channel);
    } else if (channel->isReading() || channel->isWriting()) {
        // 普通 socket → 延迟投递 IOCP IO
        pendingPosts_.insert(channel);
    }

    channels_[channel->fd()] = channel;
}

// ──── 移除 channel ──────────────────────────────────

void IocpPoller::removeChannel(Channel* channel) {
    channels_.erase(channel->fd());
    pendingPosts_.erase(channel);  // 从延迟投递集合中移除
    selectChannels_.erase(channel); // 从 select 回退集合中移除

    auto* ctx = std::any_cast<IocpContext*>(channel->context());
    if (ctx) {
        // 标记 channel 为空，poll() 中看到会跳过
        ctx->channel = nullptr;
        // 加入待清理列表，由析构函数负责释放
        deadContexts_.push_back(ctx);
        channel->context().reset();
    }

    channel->setIndex(-1);
}

// ──── 事件循环 ──────────────────────────────────────

void IocpPoller::poll(int timeoutMs, ChannelList* activeChannels) {
    // ── 延迟 IO 投递与重试 ──
    // pendingPosts_ 中的 channel 是需要投递 IO 但尚未成功投递的。
    // 每次 poll() 尝试投递，直到成功（或 channel 被移除）。
    // 此机制解决两类问题：
    //   1. 避免在 IOCP 有待处理 IO 时 CreateIoCompletionPort 误报 ERROR_INVALID_PARAMETER
    //   2. 处理非阻塞 connect：socket 连接中时 WSASend/WSARecv 返回 WSAENOTCONN，需重试
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

        // 仍有未完成的投递（如 socket 正在连接中），缩短超时以加快重试
        if (!pendingPosts_.empty() && timeoutMs > 100) {
            timeoutMs = 100;
        }
    }

    DWORD timeout = (timeoutMs < 0) ? INFINITE : static_cast<DWORD>(timeoutMs);

    // 使用 map 累积同一 channel 的多个事件，避免后续事件覆盖前一个
    std::map<Channel*, int> channelRevents;

    // 一次最多处理 128 个完成事件
    for (int i = 0; i < 128; ++i) {
        DWORD bytesTransferred = 0;
        ULONG_PTR completionKey = 0;
        OVERLAPPED* overlapped = nullptr;

        // 第一条用超时等待，后续用 0 轮询（把已完成的都取出）
        DWORD t = (i == 0) ? timeout : 0;

        BOOL ok = ::GetQueuedCompletionStatus(
            iocpHandle_,
            &bytesTransferred,
            &completionKey,
            &overlapped,
            t);

        if (overlapped == nullptr) {
            // 超时 或 IOCP 自身出错 → 退出
            break;
        }

        // ── 从 OVERLAPPED 找回 IocpContext ──
        bool isRead = false;
        IocpContext* ctx = contextFromOverlapped(overlapped, &isRead);
        if (ctx == nullptr) {
            continue;
        }

        Channel* channel = ctx->channel;

        // channel 已被移除 → 跳过
        if (channel == nullptr) {
            ctx->readPending = false;
            ctx->writePending = false;
            continue;
        }

        int revents = 0;

        if (isRead) {
            ctx->readPending = false;

            // 零字节 WSARecv：bytesTransferred 始终为 0（未请求传输数据）
            // 成功 → 数据可读；失败 (!ok) → 错误或对端关闭
            if (!ok) {
                revents |= Channel::kErrorEvent;
            } else if (channel->isReading()) {
                revents |= Channel::kReadEvent;
            }

            // Channel 仍想读且连接正常 → 重新投递读通知
            // 但如果本轮已经为该 channel 推送了读事件，跳过投递以防止
            // 0 字节 WSARecv 同步完成导致的无限循环
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
            // 写通知完成（零字节 WSASend：bytesTransferred 始终为 0）
            ctx->writePending = false;

            if (!ok) {
                revents |= Channel::kErrorEvent;
            } else if (channel->isWriting()) {
                // Channel 想写 → 触发可写事件
                // 不自动 repost：由应用层通过 enableWriting() 控制
                revents |= Channel::kWriteEvent;
            }
        }

        if (revents != 0) {
            channelRevents[channel] |= revents;
        }
    }

    // ── 统一推入 activeChannels（每个 channel 仅一次，事件已合并）──
    for (auto& [ch, rev] : channelRevents) {
        ch->setRevents(rev);
        activeChannels->push_back(ch);
    }

    // ── select 回退：处理不能用 IOCP 的 socket（如监听 socket）──
    if (!selectChannels_.empty()) {
        fd_set rfds;
        FD_ZERO(&rfds);
        int maxfd = 0;
        for (auto* ch : selectChannels_) {
            SOCKET s = (SOCKET)ch->fd();
            FD_SET(s, &rfds);
            if ((int)s > maxfd) maxfd = (int)s;
        }
        timeval tv{0, 0};  // 非阻塞轮询
        int n = ::select(maxfd + 1, &rfds, nullptr, nullptr, &tv);
        if (n > 0) {
            for (auto* ch : selectChannels_) {
                if (FD_ISSET((SOCKET)ch->fd(), &rfds) && ch->isReading()) {
                    int rev = Channel::kReadEvent;
                    // 检查是否已在 channelRevents 中
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
