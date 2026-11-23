#pragma once

#include <string>
#if MUDUO20_ENABLE_LEGACY_COMPAT
#include "muduo20/base/StringPiece.h"
#endif

#include <chrono>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string_view>

namespace muduo20 {

namespace FileUtil {
class AppendFile;
}

class LogFile {
  public:
    LogFile(std::string basename, std::int64_t rollSize, bool threadSafe = true,
            int flushInterval = 3, int checkEveryN = 1024);
    ~LogFile();

    LogFile(const LogFile &) = delete;
    LogFile &operator=(const LogFile &) = delete;

    void append(std::string_view logline);
#if MUDUO20_ENABLE_LEGACY_COMPAT
    void append(const char *logline, int len);
    void append(StringPiece logline);
    void append(StringArg logline);
#endif
    void flush();
    bool rollFile();

  private:
    void append_unlocked(std::string_view logline);

    static std::string
    getLogFileName(const std::string &basename,
                   std::chrono::system_clock::time_point *now);

    const std::string basename_;
    const std::int64_t rollSize_;
    const int flushInterval_;
    const int checkEveryN_;

    int count_ = 0;

    std::unique_ptr<std::mutex> mutex_;
    std::chrono::system_clock::time_point startOfPeriod_;
    std::chrono::system_clock::time_point lastRoll_;
    std::chrono::system_clock::time_point lastFlush_;
    std::unique_ptr<FileUtil::AppendFile> file_;

    static constexpr int kRollPerSeconds_ = 60 * 60 * 24;
};

} // namespace muduo20
