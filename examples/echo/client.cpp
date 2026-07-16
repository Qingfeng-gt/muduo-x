#include "muduox/net/EventLoop.h"
#include "muduox/net/Socket.h"
#include "muduox/net/InetAddress.h"
#include "muduox/net/Channel.h"
#include "muduox/net/SocketOps.h"
#include <iostream>
#include <string>
#include <cstring>
#include <cassert>
#include <cerrno>

int main() {
    muduox::sockets::startup();

    // ── 创建非阻塞 socket，连接服务器 ──
    muduox::Socket sock;
    muduox::InetAddress serverAddr("127.0.0.1", 8888);
    const auto& sa = serverAddr.getSockAddr();

    int ret = ::connect(sock.fd(), (const sockaddr*)&sa, sizeof(sa));
    if (ret < 0) {
#ifdef _WIN32
        if (WSAGetLastError() != WSAEWOULDBLOCK) {
            std::cerr << "connect failed: " << WSAGetLastError() << std::endl;
            return 1;
        }
#else
        if (errno != EINPROGRESS) {
            std::cerr << "connect failed: " << strerror(errno) << std::endl;
            return 1;
        }
#endif
        std::cout << "Connecting to 127.0.0.1:8888 ..." << std::endl;
    }

    muduox::EventLoop loop;
    muduox::Channel channel(&loop, sock.fd());
    std::string sendMsg = "Hello echo server!";
    bool writeDone = false;

    // ── 读回调 ──
    channel.setReadCallback([&]() {
        char buf[1024];
        int n = ::recv(sock.fd(), buf, sizeof(buf), 0);
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
            std::cerr << "recv error: " << strerror(errno) << std::endl;
            loop.quit();
        }
    });

    // ── 写回调：连接完成后发送数据 ──
    channel.setWriteCallback([&]() {
        if (writeDone) return;  // 防止重复触发

        // 检查非阻塞 connect 是否真正成功
        int err = 0;
        socklen_t len = sizeof(err);
#ifdef _WIN32
        ::getsockopt(sock.fd(), SOL_SOCKET, SO_ERROR, (char*)&err, &len);
#else
        ::getsockopt(sock.fd(), SOL_SOCKET, SO_ERROR, &err, &len);
#endif
        if (err != 0) {
            std::cerr << "Connect error: " << strerror(err) << std::endl;
            loop.quit();
            return;
        }

        writeDone = true;
        std::cout << "Connected. Sending: " << sendMsg << std::endl;

        int n = ::send(sock.fd(), sendMsg.data(),
                       static_cast<int>(sendMsg.size()),
#ifdef _WIN32
                       0
#else
                       MSG_NOSIGNAL
#endif
                       );
        if (n < 0) {
            std::cerr << "send failed: " << strerror(errno) << std::endl;
            loop.quit();
        } else {
            channel.disableWriting();
        }
    });

    channel.setErrorCallback([&]() {
        std::cerr << "Socket error (server may not be running)." << std::endl;
        loop.quit();
    });

    channel.enableWriting();
    channel.enableReading();

    std::cout << "Starting event loop..." << std::endl;
    loop.loop();

    muduox::sockets::cleanup();
    return 0;
}
