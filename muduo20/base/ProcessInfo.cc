#include "muduo20/base/ProcessInfo.h"

#include "muduo20/base/CurrentThread.h"
#include "muduo20/base/FileUtil.h"
#include "muduo20/base/Timestamp.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <cstdlib>
#include <filesystem>
#include <format>
#include <optional>
#include <pwd.h>
#include <ranges>
#include <string>
#include <string_view>
#include <sys/resource.h>
#include <sys/times.h>
#include <sys/types.h>
#include <system_error>
#include <time.h>
#include <unistd.h>
#include <vector>

using namespace muduo20;
using namespace std::string_view_literals;

namespace {

const Timestamp g_startTime = Timestamp::now();
const int g_clockTicks = static_cast<int>(::sysconf(_SC_CLK_TCK));
const int g_pageSize = static_cast<int>(::sysconf(_SC_PAGE_SIZE));

bool isNumericName(std::string_view name) {
    return !name.empty() && std::ranges::all_of(name, [](unsigned char c) {
        return std::isdigit(c) != 0;
    });
}

std::optional<int> parseInt(std::string_view sv) {
    if (sv.empty()) {
        return std::nullopt;
    }
    int value = 0;
    const auto *first = sv.data();
    const auto *last = sv.data() + sv.size();
    auto [ptr, ec] = std::from_chars(first, last, value);
    if (ec == std::errc{} && ptr == last) {
        return {value};
    }
    return std::nullopt;
}

std::string_view skipSpace(std::string_view sv) {
    while (!sv.empty() &&
           std::isspace(static_cast<unsigned char>(sv.front())) != 0) {
        sv.remove_prefix(1);
    }
    return sv;
}

} // namespace

pid_t ProcessInfo::pid() { return ::getpid(); }

std::string ProcessInfo::pidString() { return std::to_string(pid()); }

uid_t ProcessInfo::uid() { return ::getuid(); }

std::string ProcessInfo::username() {
    struct passwd pwd{};
    struct passwd *result = nullptr;
    std::array<char, 8192> buf{};
    const char *name = "unknownuser";

    ::getpwuid_r(uid(), &pwd, buf.data(), buf.size(), &result);
    if (result != nullptr) {
        name = pwd.pw_name;
    }
    return name;
}

uid_t ProcessInfo::euid() { return ::geteuid(); }

Timestamp ProcessInfo::startTime() { return g_startTime; }

int ProcessInfo::clockTicksPerSecond() { return g_clockTicks; }

int ProcessInfo::pageSize() { return g_pageSize; }

bool ProcessInfo::isDebugBuild() {
#ifdef NDEBUG
    return false;
#else
    return true;
#endif
}

std::string ProcessInfo::hostname() {
    std::array<char, 256> buf{};
    if (::gethostname(buf.data(), buf.size()) == 0) {
        buf.back() = '\0';
        return buf.data();
    }
    return "unknownhost";
}

std::string ProcessInfo::procname() {
    const auto stat = procStat();
    return std::string(procname(stat));
}

std::string_view ProcessInfo::procname(std::string_view stat) {
    const size_t lp = stat.find('(');
    const size_t rp = stat.rfind(')');
    if (lp != std::string_view::npos && rp != std::string_view::npos &&
        lp < rp) {
        return stat.substr(lp + 1, rp - lp - 1);
    }
    return {};
}

std::string_view ProcessInfo::procname(const std::string &stat) {
    return procname(std::string_view(stat));
}

std::string ProcessInfo::procStatus() {
    std::string result;
    FileUtil::readFile("/proc/self/status"sv, 65536, &result);
    return result;
}

std::string ProcessInfo::procStat() {
    std::string result;
    FileUtil::readFile("/proc/self/stat"sv, 65536, &result);
    return result;
}

std::string ProcessInfo::threadStat() {
    const std::string path =
        std::format("/proc/self/task/{}/stat", CurrentThread::tid());
    std::string result;
    FileUtil::readFile(path, 65536, &result);
    return result;
}

std::string ProcessInfo::exePath() {
    std::array<char, 1024> buf{};
    const ssize_t n = ::readlink("/proc/self/exe", buf.data(), buf.size());
    if (n > 0) {
        return std::string{buf.data(), static_cast<size_t>(n)};
    }
    return {};
}

int ProcessInfo::openedFiles() {
    int count = 0;
    std::error_code ec;
    for (const auto &entry :
         std::filesystem::directory_iterator("/proc/self/fd", ec)) {
        if (ec) {
            break;
        }
        if (isNumericName(entry.path().filename().string())) {
            ++count;
        }
    }
    return count;
}

int ProcessInfo::maxOpenFiles() {
    struct rlimit rl{};
    if (::getrlimit(RLIMIT_NOFILE, &rl) != 0) {
        return openedFiles();
    }
    return static_cast<int>(rl.rlim_cur);
}

ProcessInfo::CpuTime ProcessInfo::cpuTime() {
    CpuTime t;
    struct tms tms{};
    if (::times(&tms) >= 0) {
        const auto hz = static_cast<double>(clockTicksPerSecond());
        t.userSeconds = static_cast<double>(tms.tms_utime) / hz;
        t.systemSeconds = static_cast<double>(tms.tms_stime) / hz;
    }
    return t;
}

int ProcessInfo::numThreads() {
    const std::string status = procStatus();
    constexpr std::string_view kThreadsPrefix = "Threads:";
    for (const auto lineRange :
         std::string_view(status) | std::views::split('\n')) {
        if (std::ranges::empty(lineRange)) {
            continue;
        }
        if (const std::string_view line(std::ranges::data(lineRange),
                                        std::ranges::size(lineRange));
            !line.starts_with(kThreadsPrefix)) {
            continue;
        } else if (const auto parsed =
                       parseInt(skipSpace(line.substr(kThreadsPrefix.size())));
                   parsed.has_value()) {
            return *parsed;
        }
        return 0;
    }
    return 0;
}

std::vector<pid_t> ProcessInfo::threads() {
    std::vector<pid_t> result;
    std::error_code ec;
    for (const auto &entry :
         std::filesystem::directory_iterator("/proc/self/task", ec)) {
        if (ec) {
            break;
        }
        const std::string name = entry.path().filename().string();
        if (isNumericName(name)) {
            if (auto tid = parseInt(name); tid.has_value() && *tid > 0) {
                result.emplace_back(static_cast<pid_t>(*tid));
            }
        }
    }
    std::ranges::sort(result);
    return result;
}
