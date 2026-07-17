//
// Created by Administrator on 2026/7/15.
//

#include "Acceptor.h"
#include "muduox/net/core/EventLoop.h"
#include "InetAddress.h"
#include "SocketOps.h"
#include "muduox/base/logging/Logging.h"

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
    intptr_t connfd = acceptSocket_.accept(&peerAddr);
    if (connfd >= 0) {
        if (newConnectionCallback_) {
            newConnectionCallback_(connfd, peerAddr);
        } else {
            LOG_INFO("Acceptor: no callback set, closing fd={}", connfd);
            sockets::close(connfd);
        }
    } else {
        int err = SOCKET_GET_ERROR();
        if (err != SOCKET_EWOULDBLOCK) {
            LOG_ERROR("Acceptor::handleRead accept error: {}", err);
        }
    }
}

} // namespace muduox
