//
// Created by Administrator on 2026/7/15.
//

#ifndef MUDUOX_POLLER_H
#define MUDUOX_POLLER_H

#include "muduox/base/platform/noncopyable.h"
#include <vector>
#include <memory>

namespace muduox {

class Channel;
class EventLoop;

// IO multiplexing base class.
// Linux → EpollPoller, Windows → IocpPoller.
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

} // namespace muduox

#endif // MUDUOX_POLLER_H
