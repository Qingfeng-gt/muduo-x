//
// Created by Administrator on 2026/7/15.
//

#include "TcpServer.h"
#include "TcpConnection.h"
#include "Acceptor.h"
#include "SocketOps.h"
#include "muduox/net/core/EventLoop.h"
#include "muduox/base/logging/Logging.h"

namespace muduox {

TcpServer::TcpServer(const InetAddress& listenAddr,
                     const std::string& name, int threadNum)
    : name_(name),
      ipPort_(listenAddr),
      threadNum_(threadNum),
      acceptThread_(std::make_unique<thread::EventLoopThread>())
    {}

TcpServer::~TcpServer() {
    LOG_INFO("TcpServer [{}] destroyed", name_);
}

void TcpServer::start() {
    if (started_) return;
    started_ = true;

    auto* acceptLoop = acceptThread_->startLoop();

    if (threadNum_ > 0) {
        threadPool_ = std::make_unique<thread::EventLoopThreadPool>(acceptLoop, threadNum_);
        threadPool_->start();
    }

    acceptor_ = std::make_unique<Acceptor>(acceptLoop, ipPort_);
    acceptor_->setNewConnectionCallback([this](intptr_t sockfd, const InetAddress& peerAddr) {
        newConnection(sockfd, peerAddr);
    });

    acceptLoop->runInLoop([this]() {
        acceptor_->listen();
    });

    LOG_INFO("TcpServer [{}] started on {}", name_, ipPort_.toIpPort());
}

void TcpServer::stop() {
    if (!started_) return;
    started_ = false;

    // 先销毁 Acceptor（依赖 accept loop）
    acceptor_.reset();

    acceptThread_->stop();
}

void TcpServer::newConnection(intptr_t sockfd, const InetAddress& peerAddr) {
    auto* acceptLoop = acceptThread_->getLoop();
    acceptLoop->assertInLoopThread();

    char buf[64];
    snprintf(buf, sizeof(buf), "%s#%d", ipPort_.toIpPort().c_str(), nextConnId_);
    ++nextConnId_;
    std::string connName = name_ + "-" + buf;

    InetAddress localAddr("0.0.0.0", 0);
    sockets::getLocalAddr(sockfd, localAddr);

    auto* newLoop = threadPool_ ? threadPool_->getNextLoop() : acceptLoop;

    TcpConnectionPtr conn = std::make_shared<TcpConnection>(
        newLoop, connName, sockfd, localAddr, peerAddr);

    connections_[connName] = conn;

    LOG_INFO("TcpServer [{}] new connection [{}] from {}", name_, connName, peerAddr.toIpPort());

    conn->setConnectionCallback(
        [this](const TcpConnectionPtr& c) {
            if (!c->connected()) {
                removeConnection(c);
            }
            if (connectionCallback_) {
                connectionCallback_(c);
            }
        });
    conn->setMessageCallback(messageCallback_);
    conn->setWriteCompleteCallback(writeCompleteCallback_);

    newLoop->runInLoop([conn]() {
        conn->connectEstablished();
    });
}

void TcpServer::removeConnection(const TcpConnectionPtr& conn) {
    acceptThread_->getLoop()->runInLoop([this, conn]() {
        removeConnectionInLoop(conn);
    });
}

void TcpServer::removeConnectionInLoop(const TcpConnectionPtr& conn) {
    acceptThread_->getLoop()->assertInLoopThread();
    size_t n = connections_.erase(conn->name());
    if (n > 0) {
        LOG_INFO("TcpServer [{}] connection [{}] removed, {} alive",
                 name_, conn->name(), connections_.size());
        conn->getLoop()->queueInLoop([conn]() {
            conn->connectDestroyed();
        });
    }
}

} // namespace muduox
