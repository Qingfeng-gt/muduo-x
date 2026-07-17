//
// Created by Administrator on 2026/7/15.
//

#ifndef MUDUOX_TCPSERVER_H
#define MUDUOX_TCPSERVER_H

#include "muduox/base/platform/noncopyable.h"
#include "muduox/net/core/Callbacks.h"
#include "muduox/net/core/EventLoopThreadPool.h"
#include "InetAddress.h"
#include <map>
#include <memory>
#include <string>

namespace muduox {

class EventLoop;
class Acceptor;

// TCP server。内部管理 accept 线程 + N 个 IO worker。
//
// 用法：
//   TcpServer server(InetAddress(8888), "Echo");       // 单 accept 线程
//   TcpServer server(InetAddress(8888), "Echo", 4);     // accept + 4 IO worker
//   server.setMessageCallback(...);
//   server.start();   // 启动后立即返回，主线程自由
class TcpServer : noncopyable {
public:
    explicit TcpServer(const InetAddress& listenAddr,
                       const std::string& name = "TcpServer",
                       int threadNum = 0);
    ~TcpServer();

    const std::string& name() const { return name_; }
    const InetAddress& ipPort() const { return ipPort_; }
    EventLoop* getLoop() const { return acceptThread_->getLoop(); }

    void setConnectionCallback(ConnectionCallback cb) { connectionCallback_ = std::move(cb); }
    void setMessageCallback(MessageCallback cb)         { messageCallback_ = std::move(cb); }
    void setWriteCompleteCallback(WriteCompleteCallback cb) { writeCompleteCallback_ = std::move(cb); }

    void start();
    void stop();

private:
    void newConnection(intptr_t sockfd, const InetAddress& peerAddr);
    void removeConnection(const TcpConnectionPtr& conn);
    void removeConnectionInLoop(const TcpConnectionPtr& conn);

    const std::string name_;
    const InetAddress ipPort_;
    const int threadNum_;

    std::unique_ptr<thread::EventLoopThread> acceptThread_;
    std::unique_ptr<thread::EventLoopThreadPool> threadPool_;
    std::unique_ptr<Acceptor> acceptor_;

    ConnectionCallback    connectionCallback_;
    MessageCallback       messageCallback_;
    WriteCompleteCallback writeCompleteCallback_;

    int nextConnId_ = 1;

    using ConnectionMap = std::map<std::string, TcpConnectionPtr>;
    ConnectionMap connections_;
    bool started_ = false;
};

} // namespace muduox

#endif // MUDUOX_TCPSERVER_H
