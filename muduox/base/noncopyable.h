//
// Created by Administrator on 2026/7/15.
//

#ifndef MUDUOX_NONCOPYABLE_H
#define MUDUOX_NONCOPYABLE_H

namespace muduox {

/// 禁止拷贝和赋值 — 把拷贝构造/拷贝赋值声明为 delete
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
