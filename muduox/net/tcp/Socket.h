//
// Created by Administrator on 2026/7/15.
//

#ifndef MUDUOX_SOCKET_H
#define MUDUOX_SOCKET_H

#include "muduox/base/platform/noncopyable.h"
#include <cstdint>

namespace muduox {

class InetAddress;

// RAII wrapper for a socket fd.
class Socket : noncopyable {
public:
    explicit Socket(intptr_t sockfd) : sockfd_(sockfd) {}
    Socket();
    ~Socket();

    intptr_t fd() const { return sockfd_; }

    void bind(const InetAddress& addr);
    void listen();
    intptr_t accept(InetAddress* peerAddr);

    int connect(const InetAddress& addr);

    void shutdownWrite();

    void setReuseAddr(bool on);
    void setTcpNoDelay(bool on);
    void setKeepAlive(bool on);

private:
    const intptr_t sockfd_;
};

} // namespace muduox

#endif // MUDUOX_SOCKET_H
