//
// Created by Administrator on 2026/7/17.
//

#include "EventLoopThreadPool.h"
#include "muduox/base/logging/Logging.h"

namespace muduox::thread {

EventLoopThread::EventLoopThread() = default;

EventLoopThread::~EventLoopThread() {
    stop();
}

EventLoop* EventLoopThread::startLoop() {
    if (thread_.joinable()) return loop_.get();

    thread_ = std::thread(&EventLoopThread::threadFunc, this);

    {
        std::unique_lock lock(mutex_);
        cond_.wait(lock, [this] { return loop_ != nullptr; });
    }

    LOG_INFO("EventLoopThread started, loop={}", (void*)loop_.get());
    return loop_.get();
}

void EventLoopThread::stop() {
    if (loop_) {
        loop_->quit();
    }
    if (thread_.joinable()) {
        thread_.join();
    }
}

void EventLoopThread::threadFunc() {
    auto loop = std::make_unique<EventLoop>();

    {
        std::lock_guard lock(mutex_);
        loop_ = std::move(loop);
    }
    cond_.notify_one();

    loop_->loop();
}

// ---- EventLoopThreadPool ----

EventLoopThreadPool::EventLoopThreadPool(EventLoop* baseLoop, int numThreads)
    : baseLoop_(baseLoop),
      numThreads_(numThreads) {}

EventLoopThreadPool::~EventLoopThreadPool() {
    for (auto& t : threads_) {
        t->stop();
    }
}

void EventLoopThreadPool::start() {
    if (started_) return;
    started_ = true;

    for (int i = 0; i < numThreads_; ++i) {
        auto t = std::make_unique<EventLoopThread>();
        t->startLoop();
        threads_.push_back(std::move(t));
    }
}

EventLoop* EventLoopThreadPool::getNextLoop() {
    if (threads_.empty()) {
        return baseLoop_;
    }
    auto* loop = threads_[next_]->getLoop();
    next_ = (next_ + 1) % numThreads_;
    return loop;
}

} // namespace muduox::thread
