//
// Created by Administrator on 2026/7/15.
//

#ifndef MUDUOX_BUFFER_H
#define MUDUOX_BUFFER_H

#include "muduox/base/platform/Platform.h"

#include <vector>
#include <string>
#include <string_view>

namespace muduox {

// Application-level buffer. Uses readIndex/writeIndex to manage readable/writable regions.
class Buffer {
public:
    static constexpr size_t kInitialSize = 1024;
    static constexpr size_t kPrependSize = 8;

    Buffer() : buffer_(kPrependSize + kInitialSize), readIndex_(kPrependSize), writeIndex_(kPrependSize) {}

    const char* peek() const { return begin() + readIndex_; }
    char*       peek()       { return begin() + readIndex_; }

    size_t readableBytes() const { return writeIndex_ - readIndex_; }
    bool   empty()         const { return readableBytes() == 0; }

    void retrieve(size_t len);
    void retrieveAll();

    std::string retrieveAsString(size_t len);
    std::string retrieveAllAsString();

    void append(const char* data, size_t len);
    void append(std::string_view sv) { append(sv.data(), sv.size()); }

    ssize_t readFd(intptr_t fd, int* savedErrno);

    size_t writableBytes() const { return buffer_.size() - writeIndex_; }
    size_t prependableBytes() const { return readIndex_; }

    void ensureWritable(size_t len);

private:
    char* begin() { return buffer_.data(); }
    const char* begin() const { return buffer_.data(); }

    void makeSpace(size_t len);

    std::vector<char> buffer_;
    size_t readIndex_;
    size_t writeIndex_;
};

} // namespace muduox

#endif // MUDUOX_BUFFER_H
