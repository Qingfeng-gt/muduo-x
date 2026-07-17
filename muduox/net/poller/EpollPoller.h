//
// Created by Administrator on 2026/7/15.
//

#ifndef MUDUOX_EPOLLPOLLER_H
#define MUDUOX_EPOLLPOLLER_H

#include "muduox/net/core/Poller.h"
#include <map>
struct epoll_event;

namespace muduox {

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
    std::vector<epoll_event> events_;
    std::map<intptr_t, Channel*> channels_;
};

} // namespace muduox

#endif // MUDUOX_EPOLLPOLLER_H
