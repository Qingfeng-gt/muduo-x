//
// Created by Administrator on 2026/7/15.
//

#ifndef MUDUOX_ACCEPTOR_H
#define MUDUOX_ACCEPTOR_H

#include "muduox/base/noncopyable.h"
#include "Socket.h"
#include "Channel.h"
#include <functional>

namespace muduox {

class EventLoop;
class InetAddress;

///
/// 监听器 — 绑定端口，接受新连接。
/// 拥有 listen socket 及其 Channel，只在 EventLoop 线程中操作。
///
class Acceptor : noncopyable {
public:
    using NewConnectionCallback = std::function<void(int sockfd, const InetAddress& peerAddr)>;

    Acceptor(EventLoop* loop, const InetAddress& listenAddr);
    ~Acceptor();

    void listen();
    bool isListening() const { return listening_; }

    void setNewConnectionCallback(NewConnectionCallback cb) {
        newConnectionCallback_ = std::move(cb);
    }

private:
    void handleRead();  // Channel 可读回调 → accept

    EventLoop* loop_;
    Socket acceptSocket_;
    Channel acceptChannel_;
    bool listening_ = false;

    NewConnectionCallback newConnectionCallback_;
};

} // namespace muduox

#endif // MUDUOX_ACCEPTOR_H
