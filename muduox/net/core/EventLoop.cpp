//
// Created by Administrator on 2026/7/15.
//

#include "EventLoop.h"
#include "Channel.h"
#include "muduox/net/tcp/SocketOps.h"
#include "muduox/base/platform/Platform.h"
#include "muduox/base/logging/Logging.h"

#include <cassert>

namespace muduox {

thread_local EventLoop* t_loopInThisThread = nullptr;

static constexpr int kPollTimeMs = 10000;

EventLoop::EventLoop() {
    assert(t_loopInThisThread == nullptr);
    t_loopInThisThread = this;

    threadId_ = std::this_thread::get_id();

    poller_ = Poller::create(this);

    intptr_t ret = sockets::createTcpSocketPair(wakeupFds_);
    assert(ret == 0);  (void)ret;

    wakeupChannel_ = std::make_unique<Channel>(this, wakeupFds_[0]);
    wakeupChannel_->setReadCallback([this]() { handleWakeup(); });
    wakeupChannel_->enableReading();

    LOG_INFO("EventLoop created in thread {}", std::hash<std::thread::id>{}(threadId_));
}

EventLoop::~EventLoop() {
    wakeupChannel_->disableAll();
    wakeupChannel_->remove();

    if (wakeupFds_[0] != 0) {
        sockets::close(wakeupFds_[0]);
        sockets::close(wakeupFds_[1]);
    }

    t_loopInThisThread = nullptr;
}

void EventLoop::loop() {
    assertInLoopThread();
    looping_ = true;
    quit_ = false;

    while (!quit_) {
        activeChannels_.clear();
        poller_->poll(kPollTimeMs, &activeChannels_);

        for (Channel* channel : activeChannels_) {
            channel->handleEvent();
        }

        doPendingFunctors();
    }

    looping_ = false;
}

void EventLoop::quit() {
    quit_ = true;
    // wakeup if called from other thread
    if (!isInLoopThread()) {
        wakeup();
    }
}

void EventLoop::runInLoop(Functor cb) {
    if (isInLoopThread()) {
        cb();
    } else {
        queueInLoop(std::move(cb));
    }
}

void EventLoop::queueInLoop(Functor cb) {
    {
        std::lock_guard lock(mutex_);
        pendingFunctors_.push_back(std::move(cb));
    }

    if (!isInLoopThread()) {
        wakeup();
    }
}

void EventLoop::updateChannel(Channel* channel) {
    assert(channel->ownerLoop() == this);
    poller_->updateChannel(channel);
}

void EventLoop::removeChannel(Channel* channel) {
    assert(channel->ownerLoop() == this);
    poller_->removeChannel(channel);
}

void EventLoop::wakeup() {
    uint64_t one = 1;
    SOCKET_SEND(wakeupFds_[1], &one, sizeof(one));
}

void EventLoop::handleWakeup() {
    uint64_t buf;
    SOCKET_RECV(wakeupFds_[0], &buf, sizeof(buf));
}

void EventLoop::doPendingFunctors() {
    std::vector<Functor> functors;
    {
        std::lock_guard lock(mutex_);
        functors.swap(pendingFunctors_);
    }

    for (auto& func : functors) {
        func();
    }
}

void EventLoop::assertInLoopThread() const {
    assert(isInLoopThread());
}

} // namespace muduox
