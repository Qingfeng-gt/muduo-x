//
// Created by Administrator on 2026/7/17.
//

#include "Logging.h"

#include <chrono>
#include <ctime>

namespace muduox {

void Logger::TimeCache::update() {
    time_t now = std::time(nullptr);
    if (now != t_) {
        t_ = now;
        struct tm tm_time;
#ifdef _WIN32
        localtime_s(&tm_time, &now);
#else
        localtime_r(&now, &tm_time);
#endif
        std::snprintf(data_, kLen + 1, "%04d%02d%02d %02d:%02d:%02d",
                      tm_time.tm_year + 1900,
                      tm_time.tm_mon + 1,
                      tm_time.tm_mday,
                      tm_time.tm_hour,
                      tm_time.tm_min,
                      tm_time.tm_sec);
    }
}

void Logger::formatPrefix(const std::source_location& loc) {
    timeCache().update();
    auto now = std::chrono::system_clock::now();
    auto us  = std::chrono::duration_cast<std::chrono::microseconds>(
        now.time_since_epoch()).count() % 1000000;

    const char* fname = loc.file_name();
    const char* slash = std::strrchr(fname, '/');
    const char* bslash = std::strrchr(fname, '\\');
    if (slash) fname = slash + 1;
    if (bslash && bslash > slash) fname = bslash + 1;

    auto tid = std::hash<std::thread::id>{}(std::this_thread::get_id());

    auto prefix = std::format("{} {:06d} {} {} {}:{} - ",
                              timeCache().data_,
                              static_cast<int>(us),
                              tid,
                              kLevelNames[level_],
                              fname,
                              loc.line());
    buffer_.append(prefix.data(), prefix.size());
}

void Logger::formatMessage(const char* fmt) {
    if (fmt && *fmt) {
        buffer_.append(fmt, std::strlen(fmt));
    }
}

Logger::~Logger() {
    g_output(buffer_.data(), buffer_.length());
    if (level_ == FATAL) {
        g_flush();
        std::abort();
    }
}

} // namespace muduox
