//
// Created by Administrator on 2026/7/15.
//

#ifndef MUDUOX_NONCOPYABLE_H
#define MUDUOX_NONCOPYABLE_H

namespace muduox {

class noncopyable {
public:
    noncopyable() = default;
    ~noncopyable() = default;

    noncopyable(const noncopyable&) = delete;
    noncopyable& operator=(const noncopyable&) = delete;

    noncopyable(noncopyable&&) = default;
    noncopyable& operator=(noncopyable&&) = default;
};

} // namespace muduox

#endif // MUDUOX_NONCOPYABLE_H
