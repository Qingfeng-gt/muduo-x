//
// Created by Administrator on 2026/7/15.
//

#include "SocketOps.h"
#include "muduox/base/platform/Platform.h"
#include <cstdio>
#include <cstring>

namespace muduox::sockets {

    void startup() {
        SOCKET_INIT();
        SOCKET_IGNORE_SIGPIPE();
    }

    void cleanup() {
        SOCKET_CLEANUP();
    }

    intptr_t createTcpSocket() {
        auto sock = SOCKET_CREATE();
        if (sock == SOCKET_INVALID) return -1;
#ifdef _WIN32
        SOCKET_SET_NONBLOCK(sock);
#endif
        return static_cast<intptr_t>(sock);
    }

    intptr_t createTcpSocketPair(intptr_t fds[2]) {
#ifdef _WIN32
        // Windows 没有 socketpair，用 loopback TCP 模拟
        auto listener = SOCKET_CREATE();
        if (listener == SOCKET_INVALID) return -1;

        // 设置 SO_REUSEADDR
        BOOL reuse = TRUE;
        ::setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, (char*)&reuse, sizeof(reuse));

        // bind 到 127.0.0.1:0
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = ::htonl(INADDR_LOOPBACK);
        addr.sin_port = 0;
        if (SOCKET_BIND(listener, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR) {
            SOCKET_CLOSE(listener);
            return -1;
        }

        // 获取实际端口
        int addrLen = sizeof(addr);
        ::getsockname(listener, reinterpret_cast<sockaddr*>(&addr), &addrLen);

        // listen
        if (SOCKET_LISTEN(listener, 1) == SOCKET_ERROR) {
            SOCKET_CLOSE(listener);
            return -1;
        }

        auto client = SOCKET_CREATE();
        if (client == SOCKET_INVALID) {
            SOCKET_CLOSE(listener);
            return -1;
        }

        // 设为非阻塞后 connect
        SOCKET_SET_NONBLOCK(client);

        int connRet = SOCKET_CONNECT(client, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
        if (connRet == SOCKET_ERROR) {
            if (SOCKET_GET_ERROR() != SOCKET_EWOULDBLOCK) {
                SOCKET_CLOSE(client);
                SOCKET_CLOSE(listener);
                return -1;
            }
            // WSAEWOULDBLOCK: 连接正在进行，等待完成
            fd_set wfds;
            FD_ZERO(&wfds);
            FD_SET(client, &wfds);
            timeval tv{5, 0};
            if (::select(0, nullptr, &wfds, nullptr, &tv) <= 0) {
                SOCKET_CLOSE(client);
                SOCKET_CLOSE(listener);
                return -1;
            }
        }

        // accept 拿到对端（listener 也设非阻塞）
        SOCKET_SET_NONBLOCK(listener);
        auto server = SOCKET_ACCEPT(listener, nullptr, nullptr);
        if (server == SOCKET_INVALID) {
            SOCKET_CLOSE(client);
            SOCKET_CLOSE(listener);
            return -1;
        }

        SOCKET_CLOSE(listener);

        fds[0] = static_cast<intptr_t>(client);
        fds[1] = static_cast<intptr_t>(server);
        return 0;
#else
        // Linux: socketpair 直接创建一对已连接的本地 socket
        int fd[2];
        int ret = ::socketpair(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0, fd);
        if (ret == 0) {
            fds[0] = static_cast<intptr_t>(fd[0]);
            fds[1] = static_cast<intptr_t>(fd[1]);
        }
        return ret;
#endif
    }

    void bind(intptr_t fd, uint32_t ip, uint16_t port) {
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = ip;
        addr.sin_port = port;
        SOCKET_BIND(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
    }

    void listen(intptr_t fd) {
        SOCKET_LISTEN(fd, SOMAXCONN);
    }

    intptr_t accept(intptr_t fd, uint32_t* ip, uint16_t* port) {
        sockaddr_in addr{};
        socklen_t addrLen = sizeof(addr);
#ifdef _WIN32
        // Windows: accept 后单独设非阻塞
        SOCKET connfd = SOCKET_ACCEPT(fd, reinterpret_cast<sockaddr*>(&addr), &addrLen);
        if (connfd == SOCKET_INVALID) return -1;
        SOCKET_SET_NONBLOCK(connfd);
#else
        // Linux: accept4 一步到位（SOCKET_ACCEPT 内部用了 accept4 + SOCK_NONBLOCK）
        int connfd = SOCKET_ACCEPT(fd, reinterpret_cast<sockaddr*>(&addr), &addrLen);
        if (connfd < 0) return -1;
#endif
        if (ip) *ip = addr.sin_addr.s_addr;
        if (port) *port = addr.sin_port;
        return static_cast<intptr_t>(connfd);
    }

    void close(intptr_t fd) {
        SOCKET_CLOSE(fd);
    }

    void shutdownWrite(intptr_t fd) {
        SOCKET_SHUTDOWN_WR(fd);
    }

    void setNonBlocking(intptr_t fd) {
        SOCKET_SET_NONBLOCK(fd);
    }

    void setReuseAddr(intptr_t fd, bool on) {
        int opt = on ? 1 : 0;
        SOCKET_SETSOCKOPT(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    }

    void setTcpNoDelay(intptr_t fd, bool on) {
        int opt = on ? 1 : 0;
        SOCKET_SETSOCKOPT(fd, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt));
    }

    void setKeepAlive(intptr_t fd, bool on) {
        int opt = on ? 1 : 0;
        SOCKET_SETSOCKOPT(fd, SOL_SOCKET, SO_KEEPALIVE, &opt, sizeof(opt));
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

    void toIpPort(char* buf, size_t size, uint32_t ip, uint16_t port) {
        ::inet_ntop(AF_INET, &ip, buf, static_cast<socklen_t>(size));
        size_t len = ::strlen(buf);
        ::snprintf(buf + len, size - len, ":%u", ::ntohs(port));
    }

} // namespace muduox::sockets
