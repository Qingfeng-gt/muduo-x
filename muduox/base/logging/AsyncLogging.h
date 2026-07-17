//
// Created by Administrator on 2026/7/17.
//

#ifndef MUDUOX_ASYNCLOGGING_H
#define MUDUOX_ASYNCLOGGING_H

#include "FixedBuffer.h"
#include "LogFile.h"
#include "muduox/base/platform/noncopyable.h"
#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace muduox {

// Async logging backend using double-buffering + background thread.
class AsyncLogging : noncopyable {
public:
    AsyncLogging(const std::string& basename,
                 off_t rollSize,
                 int flushInterval = 3);
    ~AsyncLogging();

    void start();
    void stop();

    void append(const char* logline, int len);

private:
    void threadFunc(std::stop_token stoken);

    static constexpr int kLargeBuffer = 4 * 1000 * 1000; // 4MB

    using Buffer = FixedBuffer<kLargeBuffer>;
    using BufferPtr = std::unique_ptr<Buffer>;
    using BufferVector = std::vector<BufferPtr>;

    const std::string basename_;
    const off_t       rollSize_;
    const int         flushInterval_;

    std::jthread             thread_;
    std::mutex               mutex_;
    std::condition_variable  cond_;

    BufferPtr     currentBuffer_;
    BufferPtr     nextBuffer_;
    BufferVector  buffers_;
};

} // namespace muduox

#endif // MUDUOX_ASYNCLOGGING_H
