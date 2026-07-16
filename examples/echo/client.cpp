#include "muduox/net/EventLoop.h"
#include "muduox/net/Socket.h"
#include "muduox/net/InetAddress.h"
#include "muduox/net/Channel.h"
#include "muduox/net/SocketOps.h"
#include "muduox/base/Platform.h"
#include <iostream>
#include <string>
#include <cstring>
#include <cassert>

int main() {
    muduox::sockets::startup();

    // ── 1. 先创建 EventLoop（含 IOCP） ──
    muduox::EventLoop loop;

    // ── 2. 创建非阻塞 socket ──
    muduox::Socket sock;
    muduox::InetAddress serverAddr("127.0.0.1", 8888);

    // ── 3. 创建 Channel 并注册到 IOCP（必须早于 connect！） ──
    // Windows IOCP: CreateIoCompletionPort 必须在 connect() 之前调用，
    // 否则 socket 进入"正在连接"状态后 CreateIoCompletionPort 会返回 ERROR_INVALID_PARAMETER。
    muduox::Channel channel(&loop, sock.fd());
    std::string sendMsg = "Hello echo server!";
    bool writeDone = false;

    // ── 读回调 ──
    channel.setReadCallback([&]() {
        char buf[1024];
        int n = static_cast<int>(SOCKET_RECV(sock.fd(), buf, sizeof(buf)));
        if (n > 0) {
            std::string echoed(buf, n);
            std::cout << "Received: " << echoed << std::endl;
            assert(echoed == sendMsg);
            std::cout << "PASS: echo matches!" << std::endl;
            loop.quit();
        } else if (n == 0) {
            std::cout << "Server closed connection." << std::endl;
            loop.quit();
        } else {
            std::cerr << "recv error: " << SOCKET_GET_ERROR() << std::endl;
            loop.quit();
        }
    });

    // ── 写回调：连接完成后发送数据 ──
    channel.setWriteCallback([&]() {
        if (writeDone) return;

        // 检查非阻塞 connect 是否真正成功
        int err = 0;
        socklen_t len = sizeof(err);
        SOCKET_GETSOCKOPT(sock.fd(), SOL_SOCKET, SO_ERROR, &err, &len);
        if (err != 0) {
            std::cerr << "Connect error: " << err << std::endl;
            loop.quit();
            return;
        }

        writeDone = true;
        std::cout << "Connected. Sending: " << sendMsg << std::endl;

        int n = static_cast<int>(SOCKET_SEND(sock.fd(), sendMsg.data(), sendMsg.size()));
        if (n < 0) {
            std::cerr << "send failed: " << SOCKET_GET_ERROR() << std::endl;
            loop.quit();
        } else {
            channel.disableWriting();
        }
    });

    channel.setErrorCallback([&]() {
        std::cerr << "Socket error (server may not be running)." << std::endl;
        loop.quit();
    });

    // ── 注册到 IOCP（先于 connect）──
    channel.enableWriting();
    channel.enableReading();

    // ── 4. 非阻塞 connect（必须在 CreateIoCompletionPort 之后）──
    std::cout << "Connecting to 127.0.0.1:8888 ..." << std::endl;
    int ret = sock.connect(serverAddr);
    if (ret < 0) {
        int err = SOCKET_GET_ERROR();
        if (err != SOCKET_EWOULDBLOCK) {
            std::cerr << "connect failed: " << err << std::endl;
            return 1;
        }
    }



    std::cout << "Starting event loop..." << std::endl;
    loop.loop();

    muduox::sockets::cleanup();
    return 0;
}
