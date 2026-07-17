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
                     const std::string& name,int  threadNum)
    : name_(name),
      ipPort_(listenAddr),
      loop_(nullptr),
      acceptor_(nullptr),
        threadNum_(threadNum)
{
    threadPool_ = std::make_unique<thread::EventLoopThreadPool>(threadNum_);
}

TcpServer::~TcpServer() {
    LOG_INFO("TcpServer [{}] destroyed", name_);
}

void TcpServer::start() {
    if (!started_) {
        started_ = true;
        LOG_INFO("TcpServer [{}] starting on {}", name_, ipPort_.toIpPort());
        threadPool_->start();
        loop_ = threadPool_->getBaseLoop();
        if (!acceptor_) {
            acceptor_ = std::make_unique<Acceptor>(loop_,ipPort_);
            acceptor_->setNewConnectionCallback([this](intptr_t sockfd, const InetAddress& peerAddr) {
                newConnection(sockfd,peerAddr);
            });
        }
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
    sockets::getLocalAddr(sockfd, localAddr);

    auto new_loop = threadPool_->getNextLoop();

    TcpConnectionPtr conn = std::make_shared<TcpConnection>(
        new_loop, connName, sockfd, localAddr, peerAddr);

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

    new_loop->runInLoop([conn]() {
        conn->connectEstablished();
    });
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
        conn->getLoop()->queueInLoop([conn]() {
            conn->connectDestroyed();
        });
    }
}

} // namespace muduox
