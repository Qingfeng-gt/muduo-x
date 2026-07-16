//
// Created by Administrator on 2026/7/15.
//

#include "Channel.h"
#include "EventLoop.h"
#include <utility>

namespace muduox {

Channel::Channel(EventLoop* loop, int fd)
    : loop_(loop), fd_(fd) {}

Channel::~Channel() = default;

void Channel::handleEvent() {
    if (revents_ & kErrorEvent) {
        if (errorCallback_) errorCallback_();
    }
    if (revents_ & kCloseEvent) {
        if (closeCallback_) closeCallback_();
    }
    if (revents_ & kReadEvent) {
        if (readCallback_) readCallback_();
    }
    if (revents_ & kWriteEvent) {
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
