//
// Created by Administrator on 2026/7/15.
//

#ifndef MUDUOX_INETADDRESS_H
#define MUDUOX_INETADDRESS_H

#include "muduox/base/platform/copyable.h"
#include "muduox/base/platform/Platform.h"
#include <string>

namespace muduox {

class InetAddress : copyable {
public:
    explicit InetAddress(uint16_t port = 0, bool loopbackOnly = false);
    InetAddress(const std::string& ip, uint16_t port);
    explicit InetAddress(const sockaddr_in& addr);

    const sockaddr_in& getSockAddr() const { return addr_; }
    void setSockAddr(const sockaddr_in& addr) { addr_ = addr; }

    std::string toIp() const;
    uint16_t    toPort() const;
    std::string toIpPort() const;

private:
    sockaddr_in addr_;
};

} // namespace muduox

#endif // MUDUOX_INETADDRESS_H
