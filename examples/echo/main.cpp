#include "muduox/net/EventLoop.h"
#include "muduox/net/TcpServer.h"
#include "muduox/net/InetAddress.h"
#include "muduox/net/SocketOps.h"
#include "muduox/net/Callbacks.h"
#include "muduox/net/TcpConnection.h"
#include <iostream>


int main() {
    // 平台初始化：Windows → WSAStartup，Linux → 忽略 SIGPIPE
    muduox::sockets::startup();

    muduox::EventLoop loop;
    muduox::InetAddress listenAddr(8888);
    muduox::TcpServer server(&loop, listenAddr, "EchoServer");

    // 连接建立/断开回调
    server.setConnectionCallback([](const muduox::TcpConnectionPtr& conn) {
        std::cout << "[" << conn->peerAddress().toIpPort()
                  << "] " << (conn->connected() ? "UP" : "DOWN")
                  << std::endl;
    });

    // 消息回调：收到什么发回什么
    server.setMessageCallback([](const muduox::TcpConnectionPtr& conn,
                                  muduox::Buffer* buf) {
        conn->send(buf->peek(), buf->readableBytes());
        buf->retrieveAll();
    });

    server.start();
    std::cout << "Echo server listening on 0.0.0.0:8888" << std::endl;
    std::cout.flush();  // 确保立即输出，即使 stdout 被缓冲
    loop.loop();

    muduox::sockets::cleanup();
    return 0;
}
