//
// Created by Administrator on 2026/7/15.
//

#ifndef MUDUOX_BUFFER_H
#define MUDUOX_BUFFER_H

#include <cstddef>
#include <vector>
#include <string>
#include <string_view>

namespace muduox {

///
/// 应用层缓冲区。
/// 使用 readIndex / writeIndex 管理可读/可写区域，避免数据拷贝。
///
/// Layout:
///   [prependable] [readable] [writable]
///   0             ^readIndex  ^writeIndex  size()
///
class Buffer {
public:
    static constexpr size_t kInitialSize = 1024;
    static constexpr size_t kPrependSize = 8;

    Buffer() : buffer_(kPrependSize + kInitialSize), readIndex_(kPrependSize), writeIndex_(kPrependSize) {}

    // ---- 可读数据 ----
    const char* peek() const { return begin() + readIndex_; }
    char*       peek()       { return begin() + readIndex_; }

    size_t readableBytes() const { return writeIndex_ - readIndex_; }
    bool   empty()         const { return readableBytes() == 0; }

    void retrieve(size_t len);
    void retrieveAll();

    // ---- 取数据为 string ----
    std::string retrieveAsString(size_t len);
    std::string retrieveAllAsString();

    // ---- 写数据 ----
    void append(const char* data, size_t len);
    void append(std::string_view sv) { append(sv.data(), sv.size()); }

    // ---- 从 fd 读 ----
    // 返回读取的字节数，返回 0 表示对端关闭
    ssize_t readFd(int fd, int* savedErrno);

    // ---- 容量 ----
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
