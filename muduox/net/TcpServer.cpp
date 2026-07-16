//
// Created by Administrator on 2026/7/15.
//

#include "TcpServer.h"
#include "TcpConnection.h"
#include "Acceptor.h"
#include "EventLoop.h"

namespace muduox {

TcpServer::TcpServer(EventLoop* loop, const InetAddress& listenAddr,
                     const std::string& name)
    : loop_(loop),
      name_(name),
      ipPort_(listenAddr),
      acceptor_(std::make_unique<Acceptor>(loop, listenAddr))
{
    acceptor_->setNewConnectionCallback(
        [this](int sockfd, const InetAddress& peerAddr) {
            newConnection(sockfd, peerAddr);
        });
}

TcpServer::~TcpServer() = default;

void TcpServer::start() {
    if (!started_) {
        started_ = true;
        loop_->runInLoop([this]() {
            acceptor_->listen();
        });
    }
}

void TcpServer::newConnection(int sockfd, const InetAddress& peerAddr) {
    loop_->assertInLoopThread();

    char buf[64];
    snprintf(buf, sizeof(buf), "%s#%d", ipPort_.toIpPort().c_str(), nextConnId_);
    ++nextConnId_;
    std::string connName = name_ + "-" + buf;

    InetAddress localAddr("0.0.0.0", 0); // TODO: 通过 getsockname 获取真实本地地址
    TcpConnectionPtr conn = std::make_shared<TcpConnection>(
        loop_, connName, sockfd, localAddr, peerAddr);

    connections_[connName] = conn;

    // 设置回调：通知用户 + 断开时自动清理
    conn->setConnectionCallback(
        [this, connName](const TcpConnectionPtr& c) {
            if (!c->connected()) {
                removeConnection(c);
            }
            // 如果用户也设置了 connectionCallback，一并调用
            if (connectionCallback_) {
                connectionCallback_(c);
            }
        });
    conn->setMessageCallback(messageCallback_);
    conn->setWriteCompleteCallback(writeCompleteCallback_);

    // 在 loop 线程中完成连接建立
    conn->connectEstablished();
}

void TcpServer::removeConnection(const TcpConnectionPtr& conn) {
    loop_->runInLoop([this, conn]() {
        removeConnectionInLoop(conn);
    });
}

void TcpServer::removeConnectionInLoop(const TcpConnectionPtr& conn) {
    loop_->assertInLoopThread();
    size_t n = connections_.erase(conn->name());
    if (n > 0) {
        // 用 queueInLoop 延迟清理，确保当前 channel 事件处理完成后才销毁
        loop_->queueInLoop([conn]() {
            conn->connectDestroyed();
            // conn 析构 → Socket 析构 → fd close
        });
    }
}

} // namespace muduox
