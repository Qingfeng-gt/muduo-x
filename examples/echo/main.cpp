#include "muduox/net/tcp/TcpServer.h"
#include "muduox/net/tcp/InetAddress.h"
#include "muduox/net/tcp/SocketOps.h"
#include "muduox/base/logging/Logging.h"

#include <iostream>

#include "muduox/net/tcp/TcpConnection.h"

int main() {
    muduox::sockets::startup();

    muduox::InetAddress listenAddr(8888);

    // accept 线程处理新连接，4 个 IO 线程处理读写
    muduox::TcpServer server(listenAddr, "EchoServer", 4);

    server.setConnectionCallback([](const muduox::TcpConnectionPtr& conn) {
        LOG_INFO("[{}] {}",
                 conn->peerAddress().toIpPort(),
                 conn->connected() ? "UP" : "DOWN");
    });

    server.setMessageCallback([](const muduox::TcpConnectionPtr& conn,
                                  muduox::Buffer* buf) {
        conn->send(buf->peek(), buf->readableBytes());
        buf->retrieveAll();
    });

    server.start();
    LOG_INFO("Echo server listening on 0.0.0.0:8888");

    std::cout << "Press Enter to stop..." << std::endl;
    std::cin.get();

    server.stop();
    LOG_INFO("Echo server stopped");

    muduox::sockets::cleanup();
    return 0;
}
