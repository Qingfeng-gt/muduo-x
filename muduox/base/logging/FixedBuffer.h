//
// Created by Administrator on 2026/7/17.
//

#ifndef MUDUOX_FIXEDBUFFER_H
#define MUDUOX_FIXEDBUFFER_H

#include "muduox/base/platform/noncopyable.h"
#include <cstring>
#include <cassert>

namespace muduox {

// Fixed-size append-only buffer. No heap allocation.
template <int SIZE>
class FixedBuffer : noncopyable {
public:
    FixedBuffer() : cur_(data_) {}

    void append(const char* buf, size_t len) {
        int a = avail();
        if (a <= 0) return;
        size_t n = static_cast<size_t>(a);
        if (n > len) n = len;
        std::memcpy(cur_, buf, n);
        cur_ += n;
    }

    const char* data() const { return data_; }
    int length() const { return static_cast<int>(cur_ - data_); }
    char* current() { return cur_; }
    int avail() const { return static_cast<int>(end() - cur_); }
    void add(size_t len) { cur_ += len; }
    void reset() { cur_ = data_; }
    void bzero() { std::memset(data_, 0, sizeof data_); }

private:
    const char* end() const { return data_ + sizeof data_; }
    char data_[SIZE];
    char* cur_;
};

} // namespace muduox

#endif // MUDUOX_FIXEDBUFFER_H
