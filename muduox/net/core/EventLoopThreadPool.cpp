//
// Created by Administrator on 2026/7/17.
//

#include "EventLoopThreadPool.h"
#include "muduox/base/logging/Logging.h"

namespace muduox::thread {

// ---- EventLoopThread ----

EventLoopThread::EventLoopThread()
    : loop_(nullptr) {}

EventLoopThread::~EventLoopThread() {
    stop();
}

EventLoop* EventLoopThread::startLoop() {
    if (thread_.joinable()) return loop_;  // already started

    loop_ = nullptr;
    thread_ = std::thread(&EventLoopThread::threadFunc, this);

    {
        std::unique_lock lock(mutex_);
        cond_.wait(lock, [this] { return loop_ != nullptr; });
    }

    LOG_INFO("EventLoopThread started, loop={}", (void*)loop_);
    return loop_;
}

void EventLoopThread::stop() {
    if (loop_) {
        loop_->quit();  // first: tell loop to exit
    }
    if (thread_.joinable()) {
        thread_.join(); // then: wait for thread
    }
}

void EventLoopThread::threadFunc() {
    EventLoop loop;

    {
        std::lock_guard lock(mutex_);
        loop_ = &loop;
    }
    cond_.notify_one();

    loop.loop();

    {
        std::lock_guard lock(mutex_);
        loop_ = nullptr;
    }
}

// ---- EventLoopThreadPool ----

EventLoopThreadPool::EventLoopThreadPool(int numThreads)
    : numThreads_(numThreads) {}

EventLoopThreadPool::~EventLoopThreadPool() {
    for (auto& t : threads_) {
        t->stop();
    }
    if (baseThread_) {
        baseThread_->stop();
    }
}

void EventLoopThreadPool::start() {
    if (started_) return;
    started_ = true;

    // accept 线程：池自己创建
    baseThread_ = std::make_unique<EventLoopThread>();
    baseLoop_ = baseThread_->startLoop();

    // IO 工作线程
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
