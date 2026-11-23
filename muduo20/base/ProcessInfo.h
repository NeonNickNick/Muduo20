#pragma once

#include "muduo20/base/Timestamp.h"
#include "muduo20/base/Types.h"

#include <sys/types.h>

#include <string>
#include <string_view>
#include <vector>

namespace muduo20::ProcessInfo {

[[nodiscard]] pid_t pid();
[[nodiscard]] std::string pidString();
[[nodiscard]] uid_t uid();
[[nodiscard]] std::string username();
[[nodiscard]] uid_t euid();
[[nodiscard]] Timestamp startTime();
[[nodiscard]] int clockTicksPerSecond();
[[nodiscard]] int pageSize();
[[nodiscard]] bool isDebugBuild();

[[nodiscard]] std::string hostname();
[[nodiscard]] std::string procname();
[[nodiscard]] std::string_view procname(std::string_view stat);
[[nodiscard]] std::string_view procname(const std::string &stat);

[[nodiscard]] std::string procStatus();
[[nodiscard]] std::string procStat();
[[nodiscard]] std::string threadStat();

[[nodiscard]] std::string exePath();

[[nodiscard]] int openedFiles();
[[nodiscard]] int maxOpenFiles();

struct CpuTime {
    double userSeconds{0.0};
    double systemSeconds{0.0};

    [[nodiscard]] double total() const { return userSeconds + systemSeconds; }
};

[[nodiscard]] CpuTime cpuTime();

[[nodiscard]] int numThreads();
[[nodiscard]] std::vector<pid_t> threads();

} // namespace muduo20::ProcessInfo
