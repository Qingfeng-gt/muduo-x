//
// Created by Administrator on 2026/7/17.
//

#include "AsyncLogging.h"
#include <cassert>

namespace muduox {

AsyncLogging::AsyncLogging(const std::string& basename,
                           off_t rollSize,
                           int flushInterval)
    : basename_(basename)
    , rollSize_(rollSize)
    , flushInterval_(flushInterval)
    , currentBuffer_(std::make_unique<Buffer>())
    , nextBuffer_(std::make_unique<Buffer>())
{
    buffers_.reserve(16);
    currentBuffer_->bzero();
    nextBuffer_->bzero();
}

AsyncLogging::~AsyncLogging() {
    thread_.request_stop();
    cond_.notify_one();
}

void AsyncLogging::start() {
    thread_ = std::jthread([this](std::stop_token stoken) {
        threadFunc(stoken);
    });
}

void AsyncLogging::stop() {
    thread_.request_stop();
    cond_.notify_one();

}

void AsyncLogging::append(const char* logline, int len) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (currentBuffer_->avail() > len) {
        currentBuffer_->append(logline, static_cast<size_t>(len));
    } else {
        buffers_.push_back(std::move(currentBuffer_));
        if (nextBuffer_) {
            currentBuffer_ = std::move(nextBuffer_);
        } else {
            currentBuffer_ = std::make_unique<Buffer>();
        }
        currentBuffer_->append(logline, static_cast<size_t>(len));
    }
    cond_.notify_one();
}

void AsyncLogging::threadFunc(std::stop_token stoken) {
    LogFile output(basename_, rollSize_, false, flushInterval_);

    BufferPtr newBuffer1 = std::make_unique<Buffer>();
    BufferPtr newBuffer2 = std::make_unique<Buffer>();
    newBuffer1->bzero();
    newBuffer2->bzero();

    BufferVector buffersToWrite;
    buffersToWrite.reserve(16);

    while (!stoken.stop_requested()) {
        {
            std::unique_lock<std::mutex> lock(mutex_);

            cond_.wait_for(lock, std::chrono::seconds(flushInterval_));

            buffers_.push_back(std::move(currentBuffer_));
            currentBuffer_ = std::move(newBuffer1);
            buffersToWrite.swap(buffers_);

            if (!nextBuffer_) {
                nextBuffer_ = std::move(newBuffer2);
            }
        }

        for (const auto& buf : buffersToWrite) {
            if (buf->length() > 0) {
                output.append(buf->data(), buf->length());
            }
        }

        // 回收 buffer
        if (buffersToWrite.size() > 2) {
            buffersToWrite.resize(2);
        }

        if (!newBuffer1) {
            assert(!buffersToWrite.empty());
            newBuffer1 = std::move(buffersToWrite.back());
            buffersToWrite.pop_back();
            newBuffer1->reset();
        }

        if (!newBuffer2) {
            assert(!buffersToWrite.empty());
            newBuffer2 = std::move(buffersToWrite.back());
            buffersToWrite.pop_back();
            newBuffer2->reset();
        }

        buffersToWrite.clear();
    }

    // 退出循环后，处理最后一批残留数据
    {
        std::unique_lock<std::mutex> lock(mutex_);
        if (currentBuffer_->length() > 0) {
            buffers_.push_back(std::move(currentBuffer_));
            currentBuffer_ = std::move(newBuffer1);
        }
        buffersToWrite.swap(buffers_);
    }
    for (const auto& buf : buffersToWrite) {
        if (buf->length() > 0) {
            output.append(buf->data(), buf->length());
        }
    }
    output.flush();
}

} // namespace muduox
