//
// Created by Administrator on 2026/7/15.
//

#ifndef MUDUOX_TCPCONNECTION_H
#define MUDUOX_TCPCONNECTION_H

#include "muduox/base/platform/noncopyable.h"
#include "muduox/net/core/Callbacks.h"
#include "InetAddress.h"
#include "Buffer.h"
#include <memory>
#include <string>

namespace muduox {

class EventLoop;
class Socket;
class Channel;

// Manages a TCP connection lifecycle. Shared pointer ownership.
class TcpConnection : noncopyable,
                      public std::enable_shared_from_this<TcpConnection> {
public:
    enum State { kConnecting, kConnected, kDisconnecting, kDisconnected };

    TcpConnection(EventLoop* loop, const std::string& name,
                  intptr_t sockfd, const InetAddress& localAddr,
                  const InetAddress& peerAddr);
    ~TcpConnection();

    const std::string& name() const { return name_; }
    EventLoop*         getLoop() const { return loop_; }
    const InetAddress& localAddress() const { return localAddr_; }
    const InetAddress& peerAddress() const { return peerAddr_; }
    bool connected() const { return state_ == kConnected; }

    void setConnectionCallback(ConnectionCallback cb)   { connectionCallback_ = std::move(cb); }
    void setMessageCallback(MessageCallback cb)         { messageCallback_ = std::move(cb); }
    void setWriteCompleteCallback(WriteCompleteCallback cb) { writeCompleteCallback_ = std::move(cb); }

    void connectEstablished();
    void connectDestroyed();

    void send(const char* message, size_t len);
    void send(const std::string& message);
    void send(Buffer* buf);

    void shutdown();
    void forceClose();

private:
    void handleRead();
    void handleWrite();
    void handleClose();
    void handleError();

    void sendInLoop(const char* data, size_t len);
    void shutdownInLoop();

    EventLoop* loop_;
    const std::string name_;
    State state_ = kConnecting;

    std::unique_ptr<Socket> socket_;
    std::unique_ptr<Channel> channel_;

    const InetAddress localAddr_;
    const InetAddress peerAddr_;

    Buffer inputBuffer_;
    Buffer outputBuffer_;

    ConnectionCallback    connectionCallback_;
    MessageCallback       messageCallback_;
    WriteCompleteCallback writeCompleteCallback_;
};

} // namespace muduox

#endif // MUDUOX_TCPCONNECTION_H
