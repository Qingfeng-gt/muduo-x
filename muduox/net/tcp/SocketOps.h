//
// Created by Administrator on 2026/7/15.
//

#ifndef MUDUOX_SOCKETOPS_H
#define MUDUOX_SOCKETOPS_H

#include <cstdint>
#include <cstddef>

namespace muduox::sockets {
    void startup();
    void cleanup();

    intptr_t createTcpSocket();
    intptr_t createTcpSocketPair(intptr_t fds[2]);

    void bind(intptr_t fd, uint32_t ip, uint16_t port);
    void listen(intptr_t fd);
    intptr_t accept(intptr_t fd, uint32_t* ip, uint16_t* port);
    void close(intptr_t fd);
    void shutdownWrite(intptr_t fd);

    void setNonBlocking(intptr_t fd);
    void setReuseAddr(intptr_t fd, bool on);
    void setTcpNoDelay(intptr_t fd, bool on);
    void setKeepAlive(intptr_t fd, bool on);

    uint32_t hostToNetwork32(uint32_t host);
    uint16_t hostToNetwork16(uint16_t host);
    uint32_t networkToHost32(uint32_t net);
    uint16_t networkToHost16(uint16_t net);
    void     toIpPort(char* buf, size_t size, uint32_t ip, uint16_t port);
}

#endif //MUDUOX_SOCKETOPS_H
