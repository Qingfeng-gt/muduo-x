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

// 线程内创建 EventLoop（堆分配），负责其完整生命周期
class EventLoopThread : muduox::noncopyable {
public:
    EventLoopThread();
    ~EventLoopThread();

    EventLoop* startLoop();
    EventLoop* getLoop() const { return loop_.get(); }
    void stop();

private:
    void threadFunc();

    std::unique_ptr<EventLoop> loop_;
    std::thread thread_;
    std::mutex mutex_;
    std::condition_variable cond_;
};

// IO 线程池：接受外部 baseLoop（accept 线程），管理 N 个 IO worker 线程
// 当 numThreads = 0 时，getNextLoop() 返回 baseLoop_，退化为单线程模式
class EventLoopThreadPool : muduox::noncopyable {
public:
    EventLoopThreadPool(EventLoop* baseLoop, int numThreads);
    ~EventLoopThreadPool();

    void start();

    EventLoop* getBaseLoop() const { return baseLoop_; }
    EventLoop* getNextLoop();

private:
    EventLoop* baseLoop_;
    int numThreads_;
    int next_ = 0;
    std::vector<std::unique_ptr<EventLoopThread>> threads_;
    bool started_ = false;
};

} // namespace muduox::thread

#endif // MUDUO_CROSS_EVENTLOOPTHREADPOOL_H
