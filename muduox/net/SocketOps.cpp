//
// Created by Administrator on 2026/7/15.
//

#include "SocketOps.h"
#include <cstdio>
#include <cstring>

#ifdef _WIN32

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

#else

#include <csignal>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>

#endif

namespace muduox::sockets {
    void startup() {
#ifdef _WIN32
        WSADATA wsaData;
        WSAStartup(MAKEWORD(2, 2), &wsaData);
#else
        // 忽略 SIGPIPE，避免向已关闭的 socket 写入时进程被终止
        ::signal(SIGPIPE, SIG_IGN);
        std::signal(SIGPIPE, SIG_IGN);  // 双保险：C 和 C++ 版本都设置
#endif
    }

    void cleanup() {
#ifdef _WIN32
        WSACleanup();
#endif
    }

    int createTcpSocket() {
#ifdef _WIN32
        SOCKET sockfd = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (sockfd == INVALID_SOCKET) return -1;
        // Windows 需要单独设置非阻塞
        u_long mode = 1;
        ::ioctlsocket(sockfd, FIONBIO, &mode);
        return static_cast<int>(sockfd);
#else
        // Linux: 一步到位SOCK_NONBLOCK + SOCK_CLOEXEC 
        int sockfd = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, IPPROTO_TCP);
        return sockfd;
#endif
    }

    int createTcpSocketPair(int fds[2]) {
#ifdef _WIN32
        // Windows 没有 socketpair，用 loopback TCP 模拟
        // 1. 创建监听 socket
        SOCKET listener = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (listener == INVALID_SOCKET) return -1;

        // 设置 SO_REUSEADDR,允许多个进程重用同一个端口
        BOOL reuse = TRUE;
        ::setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, (char*)&reuse, sizeof(reuse));

        // 2. bind 到 127.0.0.1:0（系统自动分配端口）
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = ::htonl(INADDR_LOOPBACK);
        addr.sin_port = 0;
        if (::bind(listener, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
            ::closesocket(listener);
            return -1;
        }

        // 3. 获取实际端口
        int addrLen = sizeof(addr);
        ::getsockname(listener, (sockaddr*)&addr, &addrLen);

        // 4. listen
        if (::listen(listener, 1) == SOCKET_ERROR) {
            ::closesocket(listener);
            return -1;
        }

        // 5. 创建客户端 socket 并 connect
        SOCKET client = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        // 设为非阻塞,避免阻塞线程
        u_long noblock = 1;
        ::ioctlsocket(client, FIONBIO, &noblock);
        if (client == INVALID_SOCKET) {
            ::closesocket(listener);
            return -1;
        }

        if (::connect(client, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
            ::closesocket(client);
            ::closesocket(listener);
            return -1;
        }

        // 6. accept 拿到对端
        // listener也设置非阻塞，防止accept阻塞
        u_long lstNoblock = 1;
        ::ioctlsocket(listener, FIONBIO, &lstNoblock);
        SOCKET server = ::accept(listener, nullptr, nullptr);
        if (server == INVALID_SOCKET) {
            ::closesocket(client);
            ::closesocket(listener);
            return -1;
        }

        // 7. 关闭监听 socket
        ::closesocket(listener);


        fds[0] = static_cast<int>(client);
        fds[1] = static_cast<int>(server);
        return 0;
#else
        // Linux: socketpair 直接创建一对已连接的本地 socket
        int ret = ::socketpair(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0, fds);
        return ret;
#endif
    }

    void bind(int fd, uint32_t ip, uint16_t port) {
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        // ip 已经是网络字节序（由 InetAddress 保证），port 也已经是网络字节序
        addr.sin_addr.s_addr = ip;
        addr.sin_port = port;  // 无需 htons，port 已由 InetAddress 转为网络字节序

#ifdef _WIN32
        ::bind(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
#else
        ::bind(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
#endif
    }

    void listen(int fd) {
        ::listen(fd, SOMAXCONN);
    }

    int accept(int fd, uint32_t *ip, uint16_t *port) {
#ifdef _WIN32
        sockaddr_in addr{};
        int addrLen = sizeof(addr);
        SOCKET connfd = ::accept(fd, reinterpret_cast<sockaddr*>(&addr), &addrLen);
        if (connfd == INVALID_SOCKET) return -1;

        // 设为非阻塞
        u_long mode = 1;
        ::ioctlsocket(connfd, FIONBIO, &mode);

        if (ip) *ip = addr.sin_addr.s_addr;      // 网络字节序
        if (port) *port = addr.sin_port;         // 网络字节序
        return static_cast<int>(connfd);
#else
        sockaddr_in addr{};
        socklen_t addrLen = sizeof(addr);
        // accept4 直接拿到非阻塞 + close-on-exec 的 fd
        int connfd = ::accept4(fd, reinterpret_cast<sockaddr*>(&addr), &addrLen,
                               SOCK_NONBLOCK | SOCK_CLOEXEC);
        if (connfd < 0) return -1;

        if (ip) *ip = addr.sin_addr.s_addr;
        if (port) *port = addr.sin_port;
        return connfd;
#endif
    }

    void close(int fd) {
#ifdef _WIN32
        ::closesocket(fd);
#else
        ::close(fd);
#endif
    }

    void shutdownWrite(int fd) {
#ifdef _WIN32
        ::shutdown(fd, SD_SEND);
#else
        ::shutdown(fd, SHUT_WR);
#endif
    }

    void setNonBlocking(int fd) {
#ifdef _WIN32
        u_long noblock = 1;
        ::ioctlsocket(fd, FIONBIO, &noblock);
#else
        int flags = ::fcntl(fd, F_GETFL, 0);
        ::fcntl(fd, F_SETFL, flags | O_NONBLOCK);
#endif
    }

    void setReuseAddr(int fd, bool on) {
        int opt = on ? 1 : 0;
#ifdef _WIN32
        ::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));
#else
        ::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#endif
    }

    void setTcpNoDelay(int fd, bool on) {
        int opt = on ? 1 : 0;
#ifdef _WIN32
        ::setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (const char*)&opt, sizeof(opt));
#else
        ::setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt));
#endif
    }

    void setKeepAlive(int fd, bool on) {
        int opt = on ? 1 : 0;
#ifdef _WIN32
        ::setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, (const char*)&opt, sizeof(opt));
#else
        ::setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &opt, sizeof(opt));
#endif
    }

    uint32_t hostToNetwork32(uint32_t host) {
        return ::htonl(host);
    }

    uint16_t hostToNetwork16(uint16_t host) {
        return ::htons(host);
    }

    uint32_t networkToHost32(uint32_t net) {
        return ::ntohl(net);
    }

    uint16_t networkToHost16(uint16_t net) {
        return ::ntohs(net);
    }

    // 将网络字节序的 ip + port 拼接为 "x.x.x.x:port" 字符串
    void toIpPort(char *buf, size_t size, uint32_t ip, uint16_t port) {
        // inet_ntop 在两个平台都可用
        ::inet_ntop(AF_INET, &ip, buf, static_cast<socklen_t>(size));
        size_t len = ::strlen(buf);
        // port 从网络字节序转主机字节序后拼接到末尾
        ::snprintf(buf + len, size - len, ":%u", ::ntohs(port));
    }
} // namespace muduo::sockets
