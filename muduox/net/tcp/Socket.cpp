//
// Created by Administrator on 2026/7/15.
//

#include "Socket.h"


#include "SocketOps.h"
#include "InetAddress.h"


namespace muduox {

Socket::Socket()
    : sockfd_(sockets::createTcpSocket()) {}

Socket::~Socket() {
    sockets::close(sockfd_);
}

void Socket::bind(const InetAddress& addr) {
    const auto& sa = addr.getSockAddr();
    sockets::bind(sockfd_, sa.sin_addr.s_addr, sa.sin_port);
}

void Socket::listen() {
    sockets::listen(sockfd_);
}

intptr_t Socket::accept(InetAddress* peerAddr) {
    uint32_t ip = 0;
    uint16_t port = 0;
    intptr_t connfd = sockets::accept(sockfd_, &ip, &port);
    if (connfd >= 0 && peerAddr) {
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = ip;
        addr.sin_port = port;  // sockets::accept 返回的已是网络字节序
        peerAddr->setSockAddr(addr);
    }
    return connfd;
}

int Socket::connect(InetAddress addr) {
    auto sa = addr.getSockAddr();
    int ret = SOCKET_CONNECT(fd(), reinterpret_cast<const sockaddr*>(&sa), sizeof(sa));
    return ret;
}

void Socket::shutdownWrite() {
    sockets::shutdownWrite(sockfd_);
}

void Socket::setReuseAddr(bool on) {
    sockets::setReuseAddr(sockfd_, on);
}

void Socket::setTcpNoDelay(bool on) {
    sockets::setTcpNoDelay(sockfd_, on);
}

void Socket::setKeepAlive(bool on) {
    sockets::setKeepAlive(sockfd_, on);
}

} // namespace muduox
