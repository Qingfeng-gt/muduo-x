//
// Created by Administrator on 2026/7/17.
//

#ifndef MUDUOX_LOGFILE_H
#define MUDUOX_LOGFILE_H

#include "muduox/base/platform/noncopyable.h"
#include <memory>
#include <mutex>
#include <string>
#include <cstdio>

namespace muduox {

// Log file writer with automatic rollover.
// - Size-based: roll when file exceeds rollSize bytes
// - Time-based: roll when crossing midnight
// - Periodic flush: every flushInterval seconds or checkEveryN writes
class LogFile : noncopyable {
public:
    LogFile(const std::string& basename,
            off_t rollSize,
            bool threadSafe = true,
            int flushInterval = 3,
            int checkEveryN = 1024);
    ~LogFile();

    void append(const char* logline, int len);
    void flush();
    bool rollFile();

    off_t writtenBytes() const { return writtenBytes_; }

private:
    void append_unlocked(const char* logline, int len);
    static std::string getLogFileName(const std::string& basename, time_t* now);

    const std::string basename_;
    const off_t rollSize_;
    const int flushInterval_;
    const int checkEveryN_;

    int count_;
    std::unique_ptr<std::mutex> mutex_;
    time_t startOfPeriod_;
    time_t lastRoll_;
    time_t lastFlush_;
    std::FILE* fp_;
    off_t writtenBytes_;

    static constexpr int kRollPerSeconds = 60 * 60 * 24;
};

} // namespace muduox

#endif // MUDUOX_LOGFILE_H
