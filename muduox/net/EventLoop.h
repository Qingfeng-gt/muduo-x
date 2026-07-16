//
// Created by Administrator on 2026/7/15.
//

#ifndef MUDUOX_EVENTLOOP_H
#define MUDUOX_EVENTLOOP_H

#include "muduox/base/noncopyable.h"
#include "Poller.h"
#include <functional>
#include <memory>
#include <vector>
#include <mutex>
#include <thread>

namespace muduox {

class Channel;

///
/// 事件循环核心 — 每个线程最多一个 EventLoop。
/// 驱动 Poller 等待事件，分发活跃 Channel 的回调。
///
class EventLoop : noncopyable {
public:
    using Functor = std::function<void()>;

    EventLoop();
    ~EventLoop();

    /// 进入事件循环（阻塞当前线程）
    void loop();

    /// 退出事件循环（线程安全）
    void quit();

    /// 在 loop 线程中执行 functor
    /// - 如果调用者就在 loop 线程，立即执行
    /// - 否则放入队列并 wakeup
    void runInLoop(Functor cb);

    /// 将 functor 放入队列并 wakeup（线程安全）
    void queueInLoop(Functor cb);

    /// Poller 管理
    void updateChannel(Channel* channel);
    void removeChannel(Channel* channel);

    /// 断言当前线程是 loop 线程
    bool isInLoopThread() const { return threadId_ == std::this_thread::get_id(); }
    void assertInLoopThread() const;

private:
    void wakeup();                   // 写入 wakeupFd_，唤醒 poll
    void handleWakeup();             // 读取 wakeupFd_ 上的数据
    void doPendingFunctors();        // 执行排队的回调

    using ChannelList = std::vector<Channel*>;

    bool looping_ = false;
    bool quit_ = false;

    std::unique_ptr<Poller> poller_;

    std::thread::id threadId_;

    // ---- wakeup 机制(唤醒机制) ----
    int wakeupFds_[2]{};            // [0]=读端, [1]=写端 (socketpair)
    std::unique_ptr<Channel> wakeupChannel_;

    // ---- 活跃 channel 列表 ----
    ChannelList activeChannels_;

    // ---- 待执行的 functor ----
    std::vector<Functor> pendingFunctors_;
    std::mutex mutex_;
};

} // namespace muduox

#endif // MUDUOX_EVENTLOOP_H
