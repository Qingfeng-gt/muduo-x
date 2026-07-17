//
// Created by Administrator on 2026/7/15.
//

#ifndef MUDUOX_TCPSERVER_H
#define MUDUOX_TCPSERVER_H

#include "muduox/base/platform/noncopyable.h"
#include "muduox/net/core/Callbacks.h"
#include "InetAddress.h"
#include <map>
#include <memory>
#include <string>

#include "muduox/net/core/EventLoopThreadPool.h"

namespace muduox {

class EventLoop;
class Acceptor;

// TCP server. Manages Acceptor + all TcpConnections.
//
// Usage:
//   EventLoop loop;
//   TcpServer server(&loop, listenAddr, "name");
//   server.setConnectionCallback(...);
//   server.setMessageCallback(...);
//   server.start();
//   loop.loop();
class TcpServer : noncopyable {
public:
    explicit TcpServer(const InetAddress& listenAddr,
              const std::string& name = "TcpServer",int threadNum=4);
    ~TcpServer();

    const std::string& name() const { return name_; }
    const InetAddress& ipPort() const { return ipPort_; }

    void setConnectionCallback(ConnectionCallback cb) { connectionCallback_ = std::move(cb); }
    void setMessageCallback(MessageCallback cb)         { messageCallback_ = std::move(cb); }
    void setWriteCompleteCallback(WriteCompleteCallback cb) { writeCompleteCallback_ = std::move(cb); }

    void start();

private:
    void newConnection(intptr_t sockfd, const InetAddress& peerAddr);
    void removeConnection(const TcpConnectionPtr& conn);
    void removeConnectionInLoop(const TcpConnectionPtr& conn);

    const std::string name_;
    const InetAddress ipPort_;
    const int threadNum_;

    EventLoop *loop_;
    std::unique_ptr<Acceptor> acceptor_;

    ConnectionCallback    connectionCallback_;
    MessageCallback       messageCallback_;
    WriteCompleteCallback writeCompleteCallback_;

    int nextConnId_ = 1;

    using ConnectionMap = std::map<std::string, TcpConnectionPtr>;
    ConnectionMap connections_;
    bool started_ = false;

    std::unique_ptr<thread::EventLoopThreadPool> threadPool_;
};

} // namespace muduox

#endif // MUDUOX_TCPSERVER_H
