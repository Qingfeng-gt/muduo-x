//
// Created by Administrator on 2026/7/15.
//

#include "TcpConnection.h"
#include "EventLoop.h"
#include "Socket.h"
#include "Channel.h"
#include "SocketOps.h"
#include "muduox/base/Platform.h"

namespace muduox {

TcpConnection::TcpConnection(EventLoop* loop, const std::string& name,
                             intptr_t sockfd, const InetAddress& localAddr,
                             const InetAddress& peerAddr)
    : loop_(loop),
      name_(name),
      socket_(std::make_unique<Socket>(sockfd)),
      channel_(std::make_unique<Channel>(loop, sockfd)),
      localAddr_(localAddr),
      peerAddr_(peerAddr)
{
    channel_->setReadCallback([this]() { handleRead(); });
    channel_->setWriteCallback([this]() { handleWrite(); });
    channel_->setCloseCallback([this]() { handleClose(); });
    channel_->setErrorCallback([this]() { handleError(); });
}

TcpConnection::~TcpConnection() = default;

void TcpConnection::connectEstablished() {
    loop_->assertInLoopThread();
    state_ = kConnected;
    channel_->enableReading();  // 开始监听读事件

    if (connectionCallback_) {
        connectionCallback_(shared_from_this());
    }
}

void TcpConnection::connectDestroyed() {
    loop_->assertInLoopThread();
    channel_->disableAll();
    channel_->remove();
    socket_.reset();  // 关闭 fd
}

// ──── 读 ─────────────────────────────────────────────

void TcpConnection::handleRead() {
    int savedErrno = 0;
    ssize_t n = inputBuffer_.readFd(channel_->fd(), &savedErrno);

    if (n > 0) {
        if (messageCallback_) {
            messageCallback_(shared_from_this(), &inputBuffer_);
        }
    } else if (n == 0) {
        handleClose();
    } else {
        handleError();
    }
}

// ──── 写 ─────────────────────────────────────────────

void TcpConnection::handleWrite() {
    if (channel_->isWriting()) {
        ssize_t n = SOCKET_SEND(channel_->fd(),
                                outputBuffer_.peek(),
                                outputBuffer_.readableBytes());
        if (n > 0) {
            outputBuffer_.retrieve(n);
            if (outputBuffer_.empty()) {
                // 写完了，停止监听写事件
                channel_->disableWriting();
                if (writeCompleteCallback_) {
                    writeCompleteCallback_(shared_from_this());
                }
                // 如果正在关闭流程中，发送完剩余数据后执行半关闭
                if (state_ == kDisconnecting) {
                    socket_->shutdownWrite();
                }
            }
        } else {
            // 写出错
            handleError();
        }
    }
}

// ──── 发送 ───────────────────────────────────────────

void TcpConnection::send(const char* message, size_t len) {
    if (state_ == kConnected) {
        if (loop_->isInLoopThread()) {
            sendInLoop(message, len);
        } else {
            // 跨线程 → 复制数据到 lambda 并排队
            std::string data(message, len);
            loop_->runInLoop([this, d = std::move(data)]() {
                sendInLoop(d.data(), d.size());
            });
        }
    }
}

void TcpConnection::send(const std::string& message) {
    send(message.data(), message.size());
}

void TcpConnection::send(Buffer* buf) {
    if (state_ == kConnected) {
        if (loop_->isInLoopThread()) {
            outputBuffer_.append(buf->peek(), buf->readableBytes());
            if (!channel_->isWriting()) {
                channel_->enableWriting();
            }
        } else {
            std::string data(buf->peek(), buf->readableBytes());
            loop_->runInLoop([this, d = std::move(data)]() {
                sendInLoop(d.data(), d.size());
            });
        }
    }
}

void TcpConnection::sendInLoop(const char* data, size_t len) {
    loop_->assertInLoopThread();

    if (state_ != kConnected) return;

    // 如果当前没有待发数据（output buffer 为空），尝试直接发送
    if (!channel_->isWriting() && outputBuffer_.empty()) {
        ssize_t n = SOCKET_SEND(channel_->fd(), data, len);
        if (n >= 0) {
            if (static_cast<size_t>(n) >= len) {
                // 全部发完
                if (writeCompleteCallback_) {
                    writeCompleteCallback_(shared_from_this());
                }
                return;
            }
            // 没发完，剩余部分放入缓冲区
            data += n;
            len -= n;
        }
    }

    // 放入输出缓冲区
    outputBuffer_.append(data, len);
    if (!channel_->isWriting()) {
        channel_->enableWriting();
    }
}

// ──── 关闭 ───────────────────────────────────────────

void TcpConnection::shutdown() {
    if (state_ == kConnected) {
        state_ = kDisconnecting;
        loop_->runInLoop([this]() { shutdownInLoop(); });
    }
}

void TcpConnection::shutdownInLoop() {
    loop_->assertInLoopThread();
    if (!channel_->isWriting()) {
        // 没有待发数据，直接关闭写端
        socket_->shutdownWrite();
    }
    // 如果有待发数据，等 handleWrite() 发完再关
}

void TcpConnection::forceClose() {
    if (state_ == kConnected || state_ == kDisconnecting) {
        state_ = kDisconnecting;
        loop_->queueInLoop([this]() { handleClose(); });
    }
}

// ──── 关闭/错误 ──────────────────────────────────────

void TcpConnection::handleClose() {
    state_ = kDisconnected;
    if (connectionCallback_) {
        connectionCallback_(shared_from_this());
    }
}

void TcpConnection::handleError() {
    handleClose();
}

} // namespace muduox
