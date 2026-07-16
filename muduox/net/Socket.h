//
// Created by Administrator on 2026/7/15.
//

#ifndef MUDUOX_SOCKET_H
#define MUDUOX_SOCKET_H

#include "muduox/base/noncopyable.h"
#include <cstdint>




namespace muduox {

class InetAddress;

///
/// Socket RAII 封装。
/// 接管一个文件描述符，析构时自动关闭。
///
class Socket : noncopyable {
public:
    /// 接管已有 fd
    explicit Socket(intptr_t sockfd) : sockfd_(sockfd) {}

    /// 创建一个新的 TCP 非阻塞 socket
    Socket();

    ~Socket();

    intptr_t fd() const { return sockfd_; }

    // ---- 服务端操作 ----
    void bind(const InetAddress& addr);
    void listen();
    // 返回新连接的 fd，失败返回 -1
    intptr_t accept(InetAddress* peerAddr);

    int connect(InetAddress addr);

    // ---- 连接关闭 ----
    void shutdownWrite();

    // ---- 选项设置 ----
    void setReuseAddr(bool on);// 重复使用地址
    void setTcpNoDelay(bool on);// 禁用 Nagle 算法
    void setKeepAlive(bool on);// 保持连接

private:
    const intptr_t sockfd_;
};

} // namespace muduox

#endif // MUDUOX_SOCKET_H
