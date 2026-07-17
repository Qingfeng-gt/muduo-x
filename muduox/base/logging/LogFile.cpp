//
// Created by Administrator on 2026/7/17.
//

#ifdef _WIN32
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "LogFile.h"
#include <ctime>
#include <cassert>
#include <cstring>

#ifdef _WIN32
#include <process.h>    // _getpid
#else
#include <unistd.h>     // getpid
#endif

namespace muduox {

LogFile::LogFile(const std::string& basename,
                 off_t rollSize,
                 bool threadSafe,
                 int flushInterval,
                 int checkEveryN)
    : basename_(basename)
    , rollSize_(rollSize)
    , flushInterval_(flushInterval)
    , checkEveryN_(checkEveryN)
    , count_(0)
    , mutex_(threadSafe ? std::make_unique<std::mutex>() : nullptr)
    , startOfPeriod_(0)
    , lastRoll_(0)
    , lastFlush_(0)
    , fp_(nullptr)
    , writtenBytes_(0)
{
    assert(basename_.find('/') == std::string::npos);
    rollFile();
}

LogFile::~LogFile() {
    if (fp_) {
        std::fclose(fp_);
        fp_ = nullptr;
    }
}

std::string LogFile::getLogFileName(const std::string& basename, time_t* now) {
    std::string filename;
    filename.reserve(basename.size() + 64);
    filename = basename;

    char timeBuf[32];
    struct tm tm_time;
    *now = std::time(nullptr);
#ifdef _WIN32
    localtime_s(&tm_time, now);
#else
    localtime_r(now, &tm_time);
#endif
    std::strftime(timeBuf, sizeof timeBuf, ".%Y%m%d-%H%M%S.", &tm_time);
    filename += timeBuf;

    // 进程 ID
    filename += ".";
#ifdef _WIN32
    filename += std::to_string(_getpid());
#else
    filename += std::to_string(getpid());
#endif
    filename += ".log";
    return filename;
}

bool LogFile::rollFile() {
    time_t now = 0;
    std::string filename = getLogFileName(basename_, &now);

    // 计算当天零点时间（用于按天滚动判断）
    time_t start = now / kRollPerSeconds * kRollPerSeconds;

    if (now > lastRoll_) {
        lastRoll_ = now;
        lastFlush_ = now;
        startOfPeriod_ = start;

        if (fp_) {
            std::fclose(fp_);
        }

        // Windows 上 fopen 需要明确指定二进制/文本模式
        fp_ = std::fopen(filename.c_str(), "ae");  // append
        if (fp_) {
            // 设置 1MB 用户态缓冲区，减少系统调用
            std::setvbuf(fp_, nullptr, _IOFBF, 1024 * 1024);
            writtenBytes_ = 0;
            return true;
        }
    }
    return false;
}

void LogFile::append_unlocked(const char* logline, int len) {
    if (!fp_) return;

    size_t written = std::fwrite(logline, 1, static_cast<size_t>(len), fp_);
    writtenBytes_ += static_cast<off_t>(written);

    if (writtenBytes_ > rollSize_ && rollSize_ > 0) {
        rollFile();
    } else {
        ++count_;
        if (count_ >= checkEveryN_) {
            count_ = 0;
            time_t now = std::time(nullptr);
            time_t thisPeriod = now / kRollPerSeconds * kRollPerSeconds;
            // 跨天了就滚动
            if (thisPeriod != startOfPeriod_) {
                rollFile();
            } else if (now - lastFlush_ > flushInterval_) {
                // 定时刷新
                lastFlush_ = now;
                std::fflush(fp_);
            }
        }
    }
}

void LogFile::append(const char* logline, int len) {
    if (mutex_) {
        std::lock_guard<std::mutex> lock(*mutex_);
        append_unlocked(logline, len);
    } else {
        append_unlocked(logline, len);
    }
}

void LogFile::flush() {
    if (mutex_) {
        std::lock_guard<std::mutex> lock(*mutex_);
        if (fp_) std::fflush(fp_);
    } else {
        if (fp_) std::fflush(fp_);
    }
}

} // namespace muduox
