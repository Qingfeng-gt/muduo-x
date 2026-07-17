//
// Created by Administrator on 2026/7/15.
//

#include "EpollPoller.h"
#include "muduox/net/core/Channel.h"

#ifndef _WIN32
#include <sys/epoll.h>
#include <unistd.h>
#endif

namespace muduox {

#ifndef _WIN32

EpollPoller::EpollPoller(EventLoop* loop)
    : Poller(loop),
      epollFd_(::epoll_create1(EPOLL_CLOEXEC)),
      events_(kInitEventListSize) {}

EpollPoller::~EpollPoller() {
    ::close(epollFd_);
}

void EpollPoller::poll(int timeoutMs, ChannelList* activeChannels) {
    int numEvents = ::epoll_wait(epollFd_, events_.data(),
                                  static_cast<int>(events_.size()), timeoutMs);
    if (numEvents > 0) {
        for (int i = 0; i < numEvents; ++i) {
            auto* channel = static_cast<Channel*>(events_[i].data.ptr);
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

#endif // !_WIN32

} // namespace muduox
