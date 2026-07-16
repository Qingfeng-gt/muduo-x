//
// Created by Administrator on 2026/7/15.
//

#ifndef MUDUOX_TCPSERVER_H
#define MUDUOX_TCPSERVER_H

#include "muduox/base/noncopyable.h"
#include "Callbacks.h"
#include "InetAddress.h"
#include <map>
#include <memory>
#include <string>

namespace muduox {

class EventLoop;
class Acceptor;

///
/// TCP 服务器 — 用户接口入口。
/// 管理 Acceptor + 所有 TcpConnection，不拥有 EventLoop 线程。
///
/// 使用方式:
///   1. 创建 EventLoop + TcpServer
///   2. 设置回调 (setConnectionCallback / setMessageCallback)
///   3. server.start()
///   4. loop.loop()
///
class TcpServer : noncopyable {
public:
    TcpServer(EventLoop* loop, const InetAddress& listenAddr,
              const std::string& name = "TcpServer");
    ~TcpServer();

    const std::string& name() const { return name_; }
    const InetAddress& ipPort() const { return ipPort_; }

    // ---- 设置回调 ----
    void setConnectionCallback(ConnectionCallback cb) { connectionCallback_ = std::move(cb); }
    void setMessageCallback(MessageCallback cb)         { messageCallback_ = std::move(cb); }
    void setWriteCompleteCallback(WriteCompleteCallback cb) { writeCompleteCallback_ = std::move(cb); }

    // ---- 启动 ----
    void start();

private:
    // Acceptor 回调：接受到新连接
    void newConnection(int sockfd, const InetAddress& peerAddr);

    // 连接关闭时从列表中移除
    void removeConnection(const TcpConnectionPtr& conn);

    // 在 loop 线程中移除
    void removeConnectionInLoop(const TcpConnectionPtr& conn);

    EventLoop* loop_;
    const std::string name_;
    const InetAddress ipPort_;

    std::unique_ptr<Acceptor> acceptor_;

    ConnectionCallback    connectionCallback_;
    MessageCallback       messageCallback_;
    WriteCompleteCallback writeCompleteCallback_;

    int nextConnId_ = 1;

    // 连接表: name → TcpConnection
    using ConnectionMap = std::map<std::string, TcpConnectionPtr>;
    ConnectionMap connections_;
    bool started_ = false;
};

} // namespace muduox

#endif // MUDUOX_TCPSERVER_H
