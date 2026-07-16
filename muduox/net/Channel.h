//
// Created by Administrator on 2026/7/15.
//

#ifndef MUDUOX_CHANNEL_H
#define MUDUOX_CHANNEL_H

#include "muduox/base/noncopyable.h"
#include <functional>
#include <any>

namespace muduox {

class EventLoop;

///
/// 事件分发单元 — 绑定一个 fd 及其感兴趣的事件和回调。
/// 由 Poller 管理，Poller::poll() 返回活跃的 Channel，
/// EventLoop 调用 Channel::handleEvent() 分发回调。
///
class Channel : noncopyable {
public:
    static constexpr int kNoneEvent  = 0;
    static constexpr int kReadEvent  = 1 << 0;
    static constexpr int kWriteEvent = 1 << 1;
    static constexpr int kErrorEvent = 1 << 2;
    static constexpr int kCloseEvent = 1 << 3;

    using EventCallback = std::function<void()>;

    Channel(EventLoop* loop, int fd);
    ~Channel();

    int  fd() const { return fd_; }
    int  events() const { return events_; }
    void setRevents(int revt) { revents_ = revt; }

    // ---- 事件开关 ----
    void enableReading();
    void disableReading();
    void enableWriting();
    void disableWriting();
    void disableAll();

    bool isReading() const { return events_ & kReadEvent; }
    bool isWriting() const { return events_ & kWriteEvent; }
    bool isNoneEvent() const { return events_ == kNoneEvent; }

    // ---- 回调设置 ----
    void setReadCallback(EventCallback cb)  { readCallback_ = std::move(cb); }
    void setWriteCallback(EventCallback cb) { writeCallback_ = std::move(cb); }
    void setCloseCallback(EventCallback cb) { closeCallback_ = std::move(cb); }
    void setErrorCallback(EventCallback cb) { errorCallback_ = std::move(cb); }

    // ---- 事件分发 ----
    void handleEvent();

    // ---- Poller 管理 ----
    int  index() const { return index_; }
    void setIndex(int idx) { index_ = idx; }
    EventLoop* ownerLoop() const { return loop_; }

    void remove();  // 从 EventLoop 中移除自身

    // ---- IOCP 专用 ----
    // 通过 std::any 存储 IocpContext*，避免在头文件中暴露 Windows 类型
    std::any& context() { return context_; }
    const std::any& context() const { return context_; }

private:
    void update();  // 通知 EventLoop 更新 Poller 中的事件注册

    EventLoop* loop_;
    const int fd_;
    int events_  = kNoneEvent;
    int revents_ = kNoneEvent;
    int index_   = -1;  // Poller 内部索引（epoll: 0 表示已添加; IOCP: 类似）

    std::any context_;   // 平台相关数据（仅 Windows: IocpContext*）

    EventCallback readCallback_;
    EventCallback writeCallback_;
    EventCallback closeCallback_;
    EventCallback errorCallback_;
};

} // namespace muduox

#endif // MUDUOX_CHANNEL_H
