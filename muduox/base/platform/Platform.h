//
// Created by Administrator on 2026/7/15.
//

#ifndef MUDUOX_BASE_PLATFORM_H
#define MUDUOX_BASE_PLATFORM_H

#include <cstdint>

// Windows
#ifdef _WIN32

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

using ssize_t   = long long;
using socklen_t = int;

#define SOCKET_FD(fd)  static_cast<SOCKET>(fd)

#define SOCKET_GET_ERROR()         WSAGetLastError()
#define SOCKET_EWOULDBLOCK         WSAEWOULDBLOCK
#define SOCKET_EINPROGRESS         WSAEWOULDBLOCK
#define SOCKET_INVALID             INVALID_SOCKET

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

#define SOCKET_CREATE()  ::WSASocketW(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED)
#define SOCKET_INIT()    do { WSADATA _wsd_; WSAStartup(MAKEWORD(2,2), &_wsd_); } while(0)
#define SOCKET_CLEANUP() WSACleanup()
#define SOCKET_IGNORE_SIGPIPE()  /* nothing */

#define SOCKET_MSG_NOSIGNAL  0

// Linux / macOS
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

#define SOCKET_FD(fd)  static_cast<int>(fd)

#define SOCKET_GET_ERROR()         (errno)
#define SOCKET_EWOULDBLOCK         EAGAIN
#define SOCKET_EINPROGRESS         EINPROGRESS
#define SOCKET_INVALID             (-1)
#define SOCKET_ERROR               (-1)

#define SOCKET_CLOSE(fd)           ::close(SOCKET_FD(fd))
#define SOCKET_SEND(fd, buf, len)  ::send(SOCKET_FD(fd), (buf), (len), MSG_NOSIGNAL)
#define SOCKET_RECV(fd, buf, len)  ::recv(SOCKET_FD(fd), (buf), (len), 0)
#define SOCKET_BIND(fd, addr, len) ::bind(SOCKET_FD(fd), (addr), (len))
#define SOCKET_LISTEN(fd, bl)      ::listen(SOCKET_FD(fd), (bl))
#ifdef __linux__
#define SOCKET_ACCEPT(fd, a, al)   ::accept4(SOCKET_FD(fd), (a), (al), SOCK_NONBLOCK | SOCK_CLOEXEC)
#else
// macOS fallback: accept() + fcntl
#define SOCKET_ACCEPT(fd, a, al)   ::accept(SOCKET_FD(fd), (a), (al))
#define SOCKET_NEEDS_NONBLOCK_AFTER_ACCEPT
#endif
#define SOCKET_SHUTDOWN_WR(fd)     ::shutdown(SOCKET_FD(fd), SHUT_WR)
#define SOCKET_CONNECT(fd, a, al)  ::connect(SOCKET_FD(fd), (a), (al))

#define SOCKET_SETSOCKOPT(fd, lv, opt, val, vlen) \
    ::setsockopt(SOCKET_FD(fd), (lv), (opt), (val), (vlen))
#define SOCKET_GETSOCKOPT(fd, lv, opt, val, vlen) \
    ::getsockopt(SOCKET_FD(fd), (lv), (opt), (val), static_cast<socklen_t*>(vlen))

#ifdef SOCKET_NEEDS_NONBLOCK_AFTER_ACCEPT
#undef SOCKET_ACCEPT
#define SOCKET_ACCEPT(fd, a, al) \
    ({ \
        int _fd_ = ::accept(SOCKET_FD(fd), (a), (al)); \
        if (_fd_ >= 0) { \
            int _fl_ = ::fcntl(_fd_, F_GETFL, 0); \
            ::fcntl(_fd_, F_SETFL, _fl_ | O_NONBLOCK); \
            (void)::fcntl(_fd_, F_SETFD, FD_CLOEXEC); \
        } \
        _fd_; \
    })
#endif

#define SOCKET_SET_NONBLOCK(fd) \
    do { \
        int _fl_ = ::fcntl(SOCKET_FD(fd), F_GETFL, 0); \
        ::fcntl(SOCKET_FD(fd), F_SETFL, _fl_ | O_NONBLOCK); \
    } while(0)

#define SOCKET_CREATE()  ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, IPPROTO_TCP)
#define SOCKET_INIT()    /* nothing */
#define SOCKET_CLEANUP() /* nothing */
#define SOCKET_IGNORE_SIGPIPE() \
    do { ::signal(SIGPIPE, SIG_IGN); std::signal(SIGPIPE, SIG_IGN); } while(0)

#define SOCKET_MSG_NOSIGNAL  MSG_NOSIGNAL

#endif

#endif // MUDUOX_BASE_PLATFORM_H
