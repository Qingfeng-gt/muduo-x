//
// Created by Administrator on 2026/7/15.
//

#include "InetAddress.h"
#include "SocketOps.h"

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <netinet/in.h>
#include <arpa/inet.h>
#endif

#include <cstring>

namespace muduox {

InetAddress::InetAddress(uint16_t port, bool loopbackOnly) {
    std::memset(&addr_, 0, sizeof(addr_));
    addr_.sin_family = AF_INET;
    addr_.sin_port = sockets::hostToNetwork16(port);
    addr_.sin_addr.s_addr = loopbackOnly
        ? sockets::hostToNetwork32(INADDR_LOOPBACK)
        : sockets::hostToNetwork32(INADDR_ANY);
}

InetAddress::InetAddress(const std::string& ip, uint16_t port) {
    std::memset(&addr_, 0, sizeof(addr_));
    addr_.sin_family = AF_INET;
    addr_.sin_port = sockets::hostToNetwork16(port);
    // inet_pton 将点分十进制字符串转为网络字节序的二进制 IP
    ::inet_pton(AF_INET, ip.c_str(), &addr_.sin_addr);
}

InetAddress::InetAddress(const sockaddr_in& addr)
    : addr_(addr) {}

std::string InetAddress::toIp() const {
    char buf[INET_ADDRSTRLEN] = {};
    ::inet_ntop(AF_INET, &addr_.sin_addr, buf, sizeof(buf));
    return buf;
}

uint16_t InetAddress::toPort() const {
    return sockets::networkToHost16(addr_.sin_port);
}

std::string InetAddress::toIpPort() const {
    char buf[32] = {};
    sockets::toIpPort(buf, sizeof(buf),
                      addr_.sin_addr.s_addr,
                      addr_.sin_port);
    return buf;
}

} // namespace muduox
