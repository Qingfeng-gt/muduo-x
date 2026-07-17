//
// Created by Administrator on 2026/7/17.
//

#ifndef MUDUO_CROSS_BLOCKQUEUE_H
#define MUDUO_CROSS_BLOCKQUEUE_H

#include <queue>
#include <mutex>
#include <condition_variable>

namespace muduox {

template <typename T>
class BlockQueue {
public:
    void push(const T& x) { insert(x); }
    void push(T&& x)      { insert(std::move(x)); }

    bool pop(T& x) {
        std::unique_lock<std::mutex> lock(mutex_);
        cond_.wait(lock, [this] { return !queue_.empty() || stop_; });
        if (queue_.empty() || stop_) {
            return false;
        }
        x = std::move(queue_.front());
        queue_.pop();
        return true;
    }

    void stop() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            stop_ = true;
        }
        cond_.notify_all();
    }

private:
    template <typename... Args>
    void insert(Args&& ...args) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            queue_.emplace(std::forward<Args>(args)...);
        }
        cond_.notify_one();
    }

    std::queue<T> queue_;
    std::mutex mutex_;
    std::condition_variable cond_;
    bool stop_ = false;
};

} // namespace muduox

#endif // MUDUO_CROSS_BLOCKQUEUE_H
