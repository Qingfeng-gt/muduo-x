#include "muduox/net/core/EventLoop.h"
#include "muduox/net/tcp/Socket.h"
#include "muduox/net/tcp/InetAddress.h"
#include "muduox/net/core/Channel.h"
#include "muduox/net/tcp/SocketOps.h"
#include "muduox/base/platform/Platform.h"
#include "muduox/base/logging/Logging.h"
#include <string>
#include <cstring>
#include <cassert>

int main() {
    muduox::sockets::startup();

    muduox::EventLoop loop;
    muduox::Socket sock;
    muduox::InetAddress serverAddr("127.0.0.1", 8888);
    muduox::Channel channel(&loop, sock.fd());
    std::string sendMsg = "Hello echo server!";
    bool writeDone = false;

    channel.setReadCallback([&]() {
        char buf[1024];
        int n = static_cast<int>(SOCKET_RECV(sock.fd(), buf, sizeof(buf)));
        if (n > 0) {
            std::string echoed(buf, n);
            LOG_INFO("Received: {}", echoed);
            assert(echoed == sendMsg);
            LOG_INFO("PASS: echo matches!");
            loop.quit();
        } else if (n == 0) {
            LOG_INFO("Server closed connection.");
            loop.quit();
        } else {
            LOG_ERROR("recv error: {}", SOCKET_GET_ERROR());
            loop.quit();
        }
    });

    channel.setWriteCallback([&]() {
        if (writeDone) return;

        int err = 0;
        socklen_t len = sizeof(err);
        SOCKET_GETSOCKOPT(sock.fd(), SOL_SOCKET, SO_ERROR, &err, &len);
        if (err != 0) {
            LOG_ERROR("Connect error: {}", err);
            loop.quit();
            return;
        }

        writeDone = true;
        LOG_INFO("Connected. Sending: {}", sendMsg);

        int n = static_cast<int>(SOCKET_SEND(sock.fd(), sendMsg.data(), sendMsg.size()));
        if (n < 0) {
            LOG_ERROR("send failed: {}", SOCKET_GET_ERROR());
            loop.quit();
        } else {
            channel.disableWriting();
        }
    });

    channel.setErrorCallback([&]() {
        LOG_ERROR("Socket error (server may not be running).");
        loop.quit();
    });

    LOG_INFO("Connecting to 127.0.0.1:8888 ...");
    int ret = sock.connect(serverAddr);
    if (ret < 0) {
        int err = SOCKET_GET_ERROR();
        if (err != SOCKET_EWOULDBLOCK) {
            LOG_ERROR("connect failed: {}", err);
            return 1;
        }
    }

    channel.enableWriting();
    channel.enableReading();

    LOG_INFO("Starting event loop...");
    loop.loop();

    muduox::sockets::cleanup();
    return 0;
}
