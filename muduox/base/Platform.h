//
// 平台抽象层 — 集中所有 Windows / Linux 差异
// 其他文件只需包含此头文件，无需再写 #ifdef _WIN32
//

#ifndef MUDUOX_BASE_PLATFORM_H
#define MUDUOX_BASE_PLATFORM_H

#include <cstdint>

// ══════════════════════════════════════════════════
//  Windows
// ══════════════════════════════════════════════════
#ifdef _WIN32

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

// ── 类型 ──
using ssize_t   = long long;
using socklen_t = int;

// ── fd 转换 ──
#define SOCKET_FD(fd)  static_cast<SOCKET>(fd)

// ── 错误 ──
#define SOCKET_GET_ERROR()         WSAGetLastError()
#define SOCKET_EWOULDBLOCK         WSAEWOULDBLOCK
#define SOCKET_INVALID             INVALID_SOCKET
// SOCKET_ERROR 由 <winsock2.h> 提供

// ── 套接字操作 ──
#define SOCKET_CLOSE(fd)           ::closesocket(SOCKET_FD(fd))
#define SOCKET_SEND(fd, buf, len)  ::send(SOCKET_FD(fd), reinterpret_cast<const char*>(buf), static_cast<int>(len), 0)
#define SOCKET_RECV(fd, buf, len)  ::recv(SOCKET_FD(fd), reinterpret_cast<char*>(buf), static_cast<int>(len), 0)
#define SOCKET_BIND(fd, addr, len) ::bind(SOCKET_FD(fd), (addr), (len))
#define SOCKET_LISTEN(fd, bl)      ::listen(SOCKET_FD(fd), (bl))
#define SOCKET_ACCEPT(fd, a, al)   ::accept(SOCKET_FD(fd), (a), (al))
#define SOCKET_SHUTDOWN_WR(fd)     ::shutdown(SOCKET_FD(fd), SD_SEND)
#define SOCKET_CONNECT(fd, a, al)  ::connect(SOCKET_FD(fd), (a), (al))

#define SOCKET_SETSOCKOPT(fd, lv, opt, val, vlen) \
    ::setsockopt(SOCKET_FD(fd), (lv), (opt), reinterpret_cast<const char*>(val), (vlen))
#define SOCKET_GETSOCKOPT(fd, lv, opt, val, vlen) \
    ::getsockopt(SOCKET_FD(fd), (lv), (opt), reinterpret_cast<char*>(val), (vlen))

#define SOCKET_SET_NONBLOCK(fd) \
    do { u_long _nb_ = 1; ::ioctlsocket(SOCKET_FD(fd), FIONBIO, &_nb_); } while(0)

// ── 创建/初始化 ──
#define SOCKET_CREATE()  ::WSASocketW(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED)
#define SOCKET_INIT()    do { WSADATA _wsd_; WSAStartup(MAKEWORD(2,2), &_wsd_); } while(0)
#define SOCKET_CLEANUP() WSACleanup()
#define SOCKET_IGNORE_SIGPIPE()  /* Windows 无 SIGPIPE */

// ── 常量 ──
#define SOCKET_MSG_NOSIGNAL  0

// ══════════════════════════════════════════════════
//  Linux / macOS
// ══════════════════════════════════════════════════
#else

#include <unistd.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <csignal>
#include <signal.h>
#include <cerrno>

// ── 类型（ssize_t / socklen_t 系统自带）──

// ── fd 转换 ──
#define SOCKET_FD(fd)  static_cast<int>(fd)

// ── 错误 ──
#define SOCKET_GET_ERROR()         (errno)
#define SOCKET_EWOULDBLOCK         EINPROGRESS  // 非阻塞 connect 返回此值
#define SOCKET_INVALID             (-1)
#define SOCKET_ERROR               (-1)

// ── 套接字操作 ──
#define SOCKET_CLOSE(fd)           ::close(SOCKET_FD(fd))
#define SOCKET_SEND(fd, buf, len)  ::send(SOCKET_FD(fd), (buf), (len), MSG_NOSIGNAL)
#define SOCKET_RECV(fd, buf, len)  ::recv(SOCKET_FD(fd), (buf), (len), 0)
#define SOCKET_BIND(fd, addr, len) ::bind(SOCKET_FD(fd), (addr), (len))
#define SOCKET_LISTEN(fd, bl)      ::listen(SOCKET_FD(fd), (bl))
#define SOCKET_ACCEPT(fd, a, al)   ::accept4(SOCKET_FD(fd), (a), (al), SOCK_NONBLOCK | SOCK_CLOEXEC)
#define SOCKET_SHUTDOWN_WR(fd)     ::shutdown(SOCKET_FD(fd), SHUT_WR)
#define SOCKET_CONNECT(fd, a, al)  ::connect(SOCKET_FD(fd), (a), (al))

#define SOCKET_SETSOCKOPT(fd, lv, opt, val, vlen) \
    ::setsockopt(SOCKET_FD(fd), (lv), (opt), (val), (vlen))
#define SOCKET_GETSOCKOPT(fd, lv, opt, val, vlen) \
    ::getsockopt(SOCKET_FD(fd), (lv), (opt), (val), static_cast<socklen_t*>(vlen))

#define SOCKET_SET_NONBLOCK(fd) \
    do { \
        int _fl_ = ::fcntl(SOCKET_FD(fd), F_GETFL, 0); \
        ::fcntl(SOCKET_FD(fd), F_SETFL, _fl_ | O_NONBLOCK); \
    } while(0)

// ── 创建/初始化 ──
#define SOCKET_CREATE()  ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, IPPROTO_TCP)
#define SOCKET_INIT()    /* Linux 不需要 WSAStartup */
#define SOCKET_CLEANUP() /* Linux 不需要 WSACleanup */
#define SOCKET_IGNORE_SIGPIPE() \
    do { ::signal(SIGPIPE, SIG_IGN); std::signal(SIGPIPE, SIG_IGN); } while(0)

// ── 常量 ──
#define SOCKET_MSG_NOSIGNAL  MSG_NOSIGNAL

#endif

#endif // MUDUOX_BASE_PLATFORM_H
