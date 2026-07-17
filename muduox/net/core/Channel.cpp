//
// Created by Administrator on 2026/7/15.
//

#include "Channel.h"
#include "EventLoop.h"
#include <utility>

namespace muduox {

Channel::Channel(EventLoop* loop, intptr_t fd)
    : loop_(loop), fd_(fd) {}

Channel::~Channel() = default;

void Channel::handleEvent() {
    int revents = revents_;
    revents_ = kNoneEvent;

    if (revents & kErrorEvent) {
        if (errorCallback_) errorCallback_();
    }
    if (revents & kCloseEvent) {
        if (closeCallback_) closeCallback_();
    }
    if (revents & kReadEvent) {
        if (readCallback_) readCallback_();
    }
    if (revents & kWriteEvent) {
        if (writeCallback_) writeCallback_();
    }
}

void Channel::enableReading() {
    events_ |= kReadEvent;
    update();
}

void Channel::disableReading() {
    events_ &= ~kReadEvent;
    update();
}

void Channel::enableWriting() {
    events_ |= kWriteEvent;
    update();
}

void Channel::disableWriting() {
    events_ &= ~kWriteEvent;
    update();
}

void Channel::disableAll() {
    events_ = kNoneEvent;
    update();
}

void Channel::remove() {
    loop_->removeChannel(this);
}

void Channel::update() {
    loop_->updateChannel(this);
}

} // namespace muduox
