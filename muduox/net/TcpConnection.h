//
// Created by Administrator on 2026/7/15.
//

#ifndef MUDUOX_TCPCONNECTION_H
#define MUDUOX_TCPCONNECTION_H

#include "muduox/base/noncopyable.h"
#include "Callbacks.h"
#include "InetAddress.h"
#include "Buffer.h"
#include <memory>
#include <string>

namespace muduox {

class EventLoop;
class Socket;
class Channel;

///
/// TCP 连接 — 管理一条 TCP 连接的生命周期和数据收发。
/// 使用 shared_ptr 管理，在回调中保证生命周期。
///
class TcpConnection : noncopyable,
                      public std::enable_shared_from_this<TcpConnection> {
public:
    enum State { kConnecting, kConnected, kDisconnecting, kDisconnected };

    TcpConnection(EventLoop* loop, const std::string& name,
                  int sockfd, const InetAddress& localAddr,
                  const InetAddress& peerAddr);
    ~TcpConnection();

    const std::string& name() const { return name_; }
    EventLoop*         getLoop() const { return loop_; }
    const InetAddress& localAddress() const { return localAddr_; }
    const InetAddress& peerAddress() const { return peerAddr_; }
    bool connected() const { return state_ == kConnected; }

    // ---- 回调设置 ----
    void setConnectionCallback(ConnectionCallback cb)   { connectionCallback_ = std::move(cb); }
    void setMessageCallback(MessageCallback cb)         { messageCallback_ = std::move(cb); }
    void setWriteCompleteCallback(WriteCompleteCallback cb) { writeCompleteCallback_ = std::move(cb); }

    // ---- 连接管理 ----
    void connectEstablished();   // 新连接建立后调用（注册到 EventLoop）
    void connectDestroyed();     // 连接断开后调用（清理资源）

    // ---- 发送数据 ----
    void send(const char* message, size_t len);
    void send(const std::string& message);
    void send(Buffer* buf);

    // ---- 关闭 ----
    void shutdown();             // 优雅关闭（半关闭写端）
    void forceClose();           // 强制关闭

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

    Buffer inputBuffer_;   // 接收缓冲区
    Buffer outputBuffer_;  // 发送缓冲区

    ConnectionCallback    connectionCallback_;
    MessageCallback       messageCallback_;
    WriteCompleteCallback writeCompleteCallback_;
};

} // namespace muduox

#endif // MUDUOX_TCPCONNECTION_H
