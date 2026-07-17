//
// Created by Administrator on 2026/7/15.
//

#ifndef MUDUOX_CHANNEL_H
#define MUDUOX_CHANNEL_H

#include "muduox/base/platform/Platform.h"
#include "muduox/base/platform/noncopyable.h"
#include <functional>
#include <any>

namespace muduox {

class EventLoop;

// Binds an fd with its interested events and callbacks.
class Channel : noncopyable {
public:
    static constexpr int kNoneEvent  = 0;
    static constexpr int kReadEvent  = 1 << 0;
    static constexpr int kWriteEvent = 1 << 1;
    static constexpr int kErrorEvent = 1 << 2;
    static constexpr int kCloseEvent = 1 << 3;

    using EventCallback = std::function<void()>;

    Channel(EventLoop* loop, intptr_t fd);
    ~Channel();

    intptr_t fd() const { return fd_; }
    int  events() const { return events_; }
    void setRevents(int revt) { revents_ = revt; }

    // event control
    void enableReading();
    void disableReading();
    void enableWriting();
    void disableWriting();
    void disableAll();

    bool isReading() const { return events_ & kReadEvent; }
    bool isWriting() const { return events_ & kWriteEvent; }
    bool isNoneEvent() const { return events_ == kNoneEvent; }

    void setReadCallback(EventCallback cb)  { readCallback_ = std::move(cb); }
    void setWriteCallback(EventCallback cb) { writeCallback_ = std::move(cb); }
    void setCloseCallback(EventCallback cb) { closeCallback_ = std::move(cb); }
    void setErrorCallback(EventCallback cb) { errorCallback_ = std::move(cb); }

    void handleEvent();

    int  index() const { return index_; }
    void setIndex(int idx) { index_ = idx; }
    EventLoop* ownerLoop() const { return loop_; }

    void remove();

    // IOCP: store platform-specific context via std::any
    std::any& context() { return context_; }
    const std::any& context() const { return context_; }

private:
    void update();

    EventLoop* loop_;
    const intptr_t fd_;
    int events_  = kNoneEvent;
    int revents_ = kNoneEvent;
    int index_   = -1;

    std::any context_;

    EventCallback readCallback_;
    EventCallback writeCallback_;
    EventCallback closeCallback_;
    EventCallback errorCallback_;
};

} // namespace muduox

#endif // MUDUOX_CHANNEL_H
