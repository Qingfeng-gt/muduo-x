//
// Created by Administrator on 2026/7/15.
//

#ifndef MUDUOX_EVENTLOOP_H
#define MUDUOX_EVENTLOOP_H

#include "muduox/base/platform/noncopyable.h"
#include "Poller.h"
#include <functional>
#include <memory>
#include <vector>
#include <mutex>
#include <thread>

namespace muduox {

class Channel;

// One loop per thread. Drives Poller and dispatches active channels.
class EventLoop : noncopyable {
public:
    using Functor = std::function<void()>;

    EventLoop();
    ~EventLoop();

    void loop();
    void quit();

    void runInLoop(Functor cb);
    void queueInLoop(Functor cb);

    void updateChannel(Channel* channel);
    void removeChannel(Channel* channel);

    bool isInLoopThread() const { return threadId_ == std::this_thread::get_id(); }
    void assertInLoopThread() const;

private:
    void wakeup();
    void handleWakeup();
    void doPendingFunctors();

    using ChannelList = std::vector<Channel*>;

    bool looping_ = false;
    bool quit_ = false;

    std::unique_ptr<Poller> poller_;

    std::thread::id threadId_;

    intptr_t wakeupFds_[2]{};
    std::unique_ptr<Channel> wakeupChannel_;

    ChannelList activeChannels_;

    std::vector<Functor> pendingFunctors_;
    std::mutex mutex_;
};

} // namespace muduox

#endif // MUDUOX_EVENTLOOP_H
