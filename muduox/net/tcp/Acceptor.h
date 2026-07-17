//
// Created by Administrator on 2026/7/15.
//

#ifndef MUDUOX_ACCEPTOR_H
#define MUDUOX_ACCEPTOR_H

#include "muduox/base/platform/noncopyable.h"
#include "Socket.h"
#include "muduox/net/core/Channel.h"
#include <functional>

namespace muduox {

class EventLoop;
class InetAddress;

// Accepts new connections on a listen socket.
class Acceptor : noncopyable {
public:
    using NewConnectionCallback = std::function<void(intptr_t sockfd, const InetAddress& peerAddr)>;

    Acceptor(EventLoop* loop, const InetAddress& listenAddr);
    ~Acceptor();

    void listen();
    bool isListening() const { return listening_; }

    void setNewConnectionCallback(NewConnectionCallback cb) {
        newConnectionCallback_ = std::move(cb);
    }

private:
    void handleRead();

    EventLoop* loop_;
    Socket acceptSocket_;
    Channel acceptChannel_;
    bool listening_ = false;

    NewConnectionCallback newConnectionCallback_;
};

} // namespace muduox

#endif // MUDUOX_ACCEPTOR_H
