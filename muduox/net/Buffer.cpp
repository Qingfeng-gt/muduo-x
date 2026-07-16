//
// Created by Administrator on 2026/7/15.
//

#include "Buffer.h"

#ifdef _WIN32
#include <winsock2.h>
#else
#include <sys/uio.h>  // readv
#include <unistd.h>
#endif

#include <algorithm>
#include <cstring>

namespace muduox {

void Buffer::retrieve(size_t len) {
    if (len >= readableBytes()) {
        retrieveAll();
    } else {
        readIndex_ += len;
    }
}

void Buffer::retrieveAll() {
    readIndex_ = kPrependSize;
    writeIndex_ = kPrependSize;
}

std::string Buffer::retrieveAsString(size_t len) {
    len = std::min(len, readableBytes());
    std::string result(peek(), len);
    retrieve(len);
    return result;
}

std::string Buffer::retrieveAllAsString() {
    return retrieveAsString(readableBytes());
}

void Buffer::append(const char* data, size_t len) {
    ensureWritable(len);
    std::copy(data, data + len, begin() + writeIndex_);
    writeIndex_ += len;
}

void Buffer::ensureWritable(size_t len) {
    if (writableBytes() < len) {
        makeSpace(len);
    }
}

void Buffer::makeSpace(size_t len) {
    // 如果 [prependable + writable] 够用，整体前移
    if (prependableBytes() + writableBytes() >= len + kPrependSize) {
        size_t readable = readableBytes();
        std::copy(begin() + readIndex_,
                  begin() + writeIndex_,
                  begin() + kPrependSize);
        readIndex_ = kPrependSize;
        writeIndex_ = kPrependSize + readable;
    } else {
        // 不够用，扩容
        buffer_.resize(writeIndex_ + len);
    }
}

ssize_t Buffer::readFd(int fd, int* savedErrno) {
#ifdef _WIN32
    // Windows 没有 readv，先确保有足够空间再读
    ensureWritable(65536);
    int n = ::recv(fd, begin() + writeIndex_, static_cast<int>(writableBytes()), 0);
    if (n > 0) {
        writeIndex_ += n;
    } else if (n == 0) {
        return 0;
    } else {
        *savedErrno = WSAGetLastError();
    }
    return n;
#else
    // Linux: 使用 readv + 栈上额外缓冲区，减少扩容
    char extrabuf[65536];
    iovec vec[2];
    vec[0].iov_base = begin() + writeIndex_;
    vec[0].iov_len  = writableBytes();
    vec[1].iov_base = extrabuf;
    vec[1].iov_len  = sizeof(extrabuf);

    ssize_t n = ::readv(fd, vec, 2);
    if (n < 0) {
        *savedErrno = errno;
    } else if (static_cast<size_t>(n) <= writableBytes()) {
        writeIndex_ += n;
    } else {
        // 数据超出了内部缓冲区的剩余空间，栈上的 extrabuf 也收到了数据
        writeIndex_ = buffer_.size();
        append(extrabuf, n - writableBytes());
    }
    return n;
#endif
}

} // namespace muduox
