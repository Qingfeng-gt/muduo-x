//
// Created by Administrator on 2026/7/17.
//

#ifndef MUDUO_CROSS_EVENTLOOPTHREADPOOL_H
#define MUDUO_CROSS_EVENTLOOPTHREADPOOL_H

#include "EventLoop.h"
#include "muduox/base/platform/noncopyable.h"
#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace muduox::thread {

class EventLoopThread : muduox::noncopyable {
public:
    EventLoopThread();
    ~EventLoopThread();

    EventLoop* startLoop();
    EventLoop* getLoop() const { return loop_; }
    void stop();

private:
    void threadFunc();

    EventLoop* loop_ = nullptr;
    std::thread thread_;
    std::mutex mutex_;
    std::condition_variable cond_;
};


class EventLoopThreadPool : muduox::noncopyable {
public:
    explicit EventLoopThreadPool(int numThreads);
    ~EventLoopThreadPool();

    void start();

    EventLoop* getBaseLoop() const { return baseLoop_; }
    EventLoop* getNextLoop();

private:
    std::unique_ptr<EventLoopThread> baseThread_;
    EventLoop* baseLoop_ = nullptr;

    int numThreads_;
    int next_ = 0;
    std::vector<std::unique_ptr<EventLoopThread>> threads_;
    bool started_ = false;
};

} // namespace muduox::thread

#endif // MUDUO_CROSS_EVENTLOOPTHREADPOOL_H
