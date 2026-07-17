#include "muduox/net/core/EventLoop.h"
#include "muduox/net/tcp/TcpServer.h"
#include "muduox/net/tcp/InetAddress.h"
#include "muduox/net/tcp/SocketOps.h"
#include "muduox/net/core/Callbacks.h"
#include "muduox/net/tcp/TcpConnection.h"
#include "muduox/base/logging/Logging.h"


int main() {
    muduox::sockets::startup();

    muduox::EventLoop loop;
    muduox::InetAddress listenAddr(8888);
    muduox::TcpServer server(&loop, listenAddr, "EchoServer");

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
    loop.loop();

    muduox::sockets::cleanup();
    return 0;
}
