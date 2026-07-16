//
// Created by Administrator on 2026/7/15.
//

#include "Acceptor.h"
#include "EventLoop.h"
#include "InetAddress.h"
#include "SocketOps.h"

namespace muduox {

Acceptor::Acceptor(EventLoop* loop, const InetAddress& listenAddr)
    : loop_(loop),
      acceptSocket_(),
      acceptChannel_(loop, acceptSocket_.fd())
{
    acceptSocket_.setReuseAddr(true);
    acceptSocket_.bind(listenAddr);

    acceptChannel_.setReadCallback([this]() { handleRead(); });
}

Acceptor::~Acceptor() {
    acceptChannel_.disableAll();
    acceptChannel_.remove();
}

void Acceptor::listen() {
    loop_->assertInLoopThread();
    listening_ = true;
    acceptSocket_.listen();
    acceptChannel_.enableReading();
}

void Acceptor::handleRead() {
    InetAddress peerAddr;
    int connfd = acceptSocket_.accept(&peerAddr);
    if (connfd >= 0) {
        if (newConnectionCallback_) {
            newConnectionCallback_(connfd, peerAddr);
        } else {
            sockets::close(connfd);
        }
    }
    // EAGAIN 等可恢复错误直接忽略，等待下次事件
}

} // namespace muduox
