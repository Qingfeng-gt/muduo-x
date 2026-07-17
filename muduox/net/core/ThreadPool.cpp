//
// Created by Administrator on 2026/7/17.
//

#include "ThreadPool.h"
namespace muduox::thread {
    ThreadPool::ThreadPool(int numThreads)
        : numThreads_(std::max(1, numThreads)),
        taskQueue_(std::make_unique<BlockQueue<Task>>())
    {
        for (int i = 0; i < numThreads_; ++i) {
            threads_.emplace_back(&ThreadPool::work,this);
        }
    }

    ThreadPool::~ThreadPool() {
        taskQueue_->stop();
        for (auto& thread : threads_) {
            if (thread.joinable()) {
                thread.join();
            }
        }
    }

    void ThreadPool::push(std::function<void()> task) {
        taskQueue_->push(std::move(task));
    }

    void ThreadPool::work() {
        Task task;
        while (taskQueue_->pop(task)) {
            if (task) {
                task();
            }
        }
    }
}