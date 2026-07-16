//
// Created by Administrator on 2026/7/15.
//

#ifndef MUDUOX_POLLER_H
#define MUDUOX_POLLER_H

#include "muduox/base/noncopyable.h"
#include <vector>
#include <memory>

namespace muduox {

class Channel;
class EventLoop;

///
/// IO 多路复用抽象基类。
/// 不同平台实现不同：
///   - Linux: EpollPoller (epoll_create / epoll_ctl / epoll_wait)
///   - Windows: IocpPoller (CreateIoCompletionPort / GetQueuedCompletionStatus)
///
class Poller : noncopyable {
public:
    using ChannelList = std::vector<Channel*>;

    explicit Poller(EventLoop* loop) : ownerLoop_(loop) {}
    virtual ~Poller() = default;

    /// 等待事件发生，超时返回空列表
    virtual void poll(int timeoutMs, ChannelList* activeChannels) = 0;

    /// 更新 channel 的事件监听（ADD / MOD）
    virtual void updateChannel(Channel* channel) = 0;

    /// 移除 channel 的事件监听（DEL）
    virtual void removeChannel(Channel* channel) = 0;

    /// 工厂方法：根据平台创建对应的 Poller 实现
    static std::unique_ptr<Poller> create(EventLoop* loop);

protected:
    EventLoop* ownerLoop_;
};

} // namespace muduox

#endif // MUDUOX_POLLER_H
