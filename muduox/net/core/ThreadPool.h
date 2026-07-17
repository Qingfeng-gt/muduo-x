//
// Created by Administrator on 2026/7/17.
//

#ifndef MUDUO_CROSS_THREADPOOL_H
#define MUDUO_CROSS_THREADPOOL_H
#include <functional>
#include <thread>
#include <vector>

#include "BlockQueue.h"
#include "muduox/base/platform/noncopyable.h"
#include <algorithm>

namespace muduox::thread {
    class ThreadPool: muduox::noncopyable {
    public:
        explicit ThreadPool(int numThreads);
        ~ThreadPool();

        void push(std::function<void()> task);

        template <typename... Args>
        void push(Args&& ...args) {
            taskQueue_->push(std::forward<Args>(args)...);
        }
    private:
        void work();

        int numThreads_;
        using Task = std::function<void()>;
        std::vector<std::thread> threads_;
        std::unique_ptr<BlockQueue<Task>> taskQueue_;
    };
}
#endif //MUDUO_CROSS_THREADPOOL_H