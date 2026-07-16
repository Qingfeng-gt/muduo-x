//
// Created by Administrator on 2026/7/15.
//

#include "EventLoop.h"
#include "Channel.h"
#include "SocketOps.h"
#include "muduox/base/Platform.h"

#include <cassert>

namespace muduox {

// 线程局部存储：当前线程的 EventLoop（确保 one loop per thread）
thread_local EventLoop* t_loopInThisThread = nullptr;

// poll 超时时间（毫秒）。用非无限超时避免 poll 永远阻塞
static constexpr int kPollTimeMs = 10000;

EventLoop::EventLoop() {
    // 确保当前线程还没有 EventLoop（one loop per thread）
    assert(t_loopInThisThread == nullptr);
    t_loopInThisThread = this;

    threadId_ = std::this_thread::get_id();

    // 创建 Poller
    poller_ = Poller::create(this);

    // 创建 wakeup 用的 socketpair
    intptr_t ret = sockets::createTcpSocketPair(wakeupFds_);
    assert(ret == 0);  (void)ret;

    // 创建 wakeup channel，监听 wakeupFds_[0] 的可读事件
    wakeupChannel_ = std::make_unique<Channel>(this, wakeupFds_[0]);
    wakeupChannel_->setReadCallback([this]() { handleWakeup(); });
    wakeupChannel_->enableReading();
}

EventLoop::~EventLoop() {
    wakeupChannel_->disableAll();
    wakeupChannel_->remove();

    sockets::close(wakeupFds_[0]);
    sockets::close(wakeupFds_[1]);

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
    // 如果不在 loop 线程，需要 wakeup 让 poll 返回
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

    // 不在 loop 线程时需要 wakeup；在 loop 线程但正在处理 pending functors 时也需要
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

// ---- 添加 assertInLoopThread 辅助 ----
void EventLoop::assertInLoopThread() const {
    assert(isInLoopThread());
}

} // namespace muduox
