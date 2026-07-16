//
// Created by Administrator on 2026/7/15.
//

#ifndef MUDUOX_INETADDRESS_H
#define MUDUOX_INETADDRESS_H

#include "muduox/base/copyable.h"
#include "muduox/base/Platform.h"
#include <string>

namespace muduox {

///
/// IP 地址 + 端口封装。
/// 内部使用网络字节序存储，可直接传给 socket API。
///
class InetAddress : copyable {
public:
    /// 构造通配地址，监听所有网卡
    explicit InetAddress(uint16_t port = 0, bool loopbackOnly = false);

    /// 从 IP 字符串 + 端口构造，如 InetAddress("192.168.1.1", 8080)
    InetAddress(const std::string& ip, uint16_t port);

    /// 从 sockaddr_in 构造（网络字节序）
    explicit InetAddress(const sockaddr_in& addr);

    // ---- 基础 getter ----
    const sockaddr_in& getSockAddr() const { return addr_; }
    void setSockAddr(const sockaddr_in& addr) { addr_ = addr; }

    std::string toIp() const;
    uint16_t    toPort() const;       // 主机字节序
    std::string toIpPort() const;     // "x.x.x.x:port"

private:
    sockaddr_in addr_;
};

} // namespace muduox

#endif // MUDUOX_INETADDRESS_H
