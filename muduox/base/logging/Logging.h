//
// Created by Administrator on 2026/7/17.
//

#ifndef MUDUOX_LOGGING_H
#define MUDUOX_LOGGING_H

#include "FixedBuffer.h"
#include <atomic>
#include <format>
#include <functional>
#include <source_location>
#include <chrono>
#include <cstring>
#include <cstdio>
#include <thread>

namespace muduox {

class Logger {
public:
    // Windows <wingdi.h> 定义了 ERROR 宏，先取消
#ifdef ERROR
#undef ERROR
#endif
    enum LogLevel { TRACE, DEBUG, INFO, WARN, ERROR, FATAL, NUM_LOG_LEVELS };

    static void defaultOutput(const char* msg, int len) {
        std::fwrite(msg, 1, static_cast<size_t>(len), stdout);
        std::fflush(stdout);
    }
    static void defaultFlush() { std::fflush(stdout); }

    using OutputFunc = std::function<void(const char*, int)>;
    using FlushFunc   = std::function<void()>;

    // setOutput/setFlush are not thread-safe against concurrent logging.
    // Call them once at startup, before starting any logging threads.
    static void   setOutput(OutputFunc f) { g_output   = std::move(f); }
    static void   setFlush(FlushFunc f)   { g_flush    = std::move(f); }
    static LogLevel logLevel()            { return g_logLevel.load(std::memory_order_relaxed); }
    static void   setLogLevel(LogLevel l) { g_logLevel.store(l, std::memory_order_relaxed); }

    // source_location 由日志宏传入
    template <typename... Args>
    Logger(LogLevel level,
           std::source_location loc,
           const char* fmt = "", Args&&... args)
        : level_(level)
    {
        formatPrefix(loc);
        formatMessage(fmt, std::forward<Args>(args)...);
        buffer_.append("\n", 1);
    }

    ~Logger();

private:
    void formatPrefix(const std::source_location& loc);
    void formatMessage(const char* fmt);  // 无参数版本

    template <typename... Args>
    void formatMessage(const char* fmt, Args&&... args) {
        auto msg = std::vformat(fmt, std::make_format_args(args...));
        buffer_.append(msg.data(), msg.size());
    }

    inline static OutputFunc g_output   = defaultOutput;
    inline static FlushFunc   g_flush   = defaultFlush;
    inline static std::atomic<LogLevel> g_logLevel = INFO;

    // 每线程缓存秒级时间串
    struct TimeCache {
        void update();
        time_t t_ = 0;
        static constexpr int kLen = 24;
        char data_[kLen + 1] = {};
    };
    inline static TimeCache& timeCache() {
        thread_local TimeCache tc;
        return tc;
    }

    static constexpr const char* kLevelNames[NUM_LOG_LEVELS] = {
        "TRACE", "DEBUG", "INFO ", "WARN ", "ERROR", "FATAL",
    };

    static constexpr int kBufferSize = 4000;
    using Buffer = FixedBuffer<kBufferSize>;
    Buffer    buffer_;
    LogLevel  level_;
};

} // namespace muduox

#define LOG_TRACE(...) \
    if (muduox::Logger::logLevel() <= muduox::Logger::TRACE) \
        muduox::Logger(muduox::Logger::TRACE, std::source_location::current(), __VA_ARGS__)

#define LOG_DEBUG(...) \
    if (muduox::Logger::logLevel() <= muduox::Logger::DEBUG) \
        muduox::Logger(muduox::Logger::DEBUG, std::source_location::current(), __VA_ARGS__)

#define LOG_INFO(...) \
    if (muduox::Logger::logLevel() <= muduox::Logger::INFO) \
        muduox::Logger(muduox::Logger::INFO, std::source_location::current(), __VA_ARGS__)

#define LOG_WARN(...)  muduox::Logger(muduox::Logger::WARN,  std::source_location::current(), __VA_ARGS__)
#define LOG_ERROR(...) muduox::Logger(muduox::Logger::ERROR, std::source_location::current(), __VA_ARGS__)
#define LOG_FATAL(...) muduox::Logger(muduox::Logger::FATAL, std::source_location::current(), __VA_ARGS__)

#endif // MUDUOX_LOGGING_H
