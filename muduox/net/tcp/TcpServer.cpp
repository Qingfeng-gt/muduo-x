//
// Created by Administrator on 2026/7/15.
//

#include "TcpServer.h"
#include "TcpConnection.h"
#include "Acceptor.h"
#include "muduox/net/core/EventLoop.h"
#include "muduox/base/logging/Logging.h"

namespace muduox {

TcpServer::TcpServer(EventLoop* loop, const InetAddress& listenAddr,
                     const std::string& name)
    : loop_(loop),
      name_(name),
      ipPort_(listenAddr),
      acceptor_(std::make_unique<Acceptor>(loop, listenAddr))
{
    acceptor_->setNewConnectionCallback(
        [this](intptr_t sockfd, const InetAddress& peerAddr) {
            newConnection(sockfd, peerAddr);
        });
}

TcpServer::~TcpServer() {
    LOG_INFO("TcpServer [{}] destroyed", name_);
}

void TcpServer::start() {
    if (!started_) {
        started_ = true;
        LOG_INFO("TcpServer [{}] starting on {}", name_, ipPort_.toIpPort());
        loop_->runInLoop([this]() {
            acceptor_->listen();
        });
    }
}

void TcpServer::newConnection(intptr_t sockfd, const InetAddress& peerAddr) {
    loop_->assertInLoopThread();

    char buf[64];
    snprintf(buf, sizeof(buf), "%s#%d", ipPort_.toIpPort().c_str(), nextConnId_);
    ++nextConnId_;
    std::string connName = name_ + "-" + buf;

    InetAddress localAddr("0.0.0.0", 0);
    TcpConnectionPtr conn = std::make_shared<TcpConnection>(
        loop_, connName, sockfd, localAddr, peerAddr);

    connections_[connName] = conn;

    LOG_INFO("TcpServer [{}] new connection [{}] from {}", name_, connName, peerAddr.toIpPort());

    conn->setConnectionCallback(
        [this, connName](const TcpConnectionPtr& c) {
            if (!c->connected()) {
                removeConnection(c);
            }
            if (connectionCallback_) {
                connectionCallback_(c);
            }
        });
    conn->setMessageCallback(messageCallback_);
    conn->setWriteCompleteCallback(writeCompleteCallback_);

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
        LOG_INFO("TcpServer [{}] connection [{}] removed, {} alive", name_, conn->name(), connections_.size());
        loop_->queueInLoop([conn]() {
            conn->connectDestroyed();
        });
    }
}

} // namespace muduox
