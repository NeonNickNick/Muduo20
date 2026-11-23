#pragma once

#include <array>
#include <bits/pthreadtypes.h>
#include <cstddef>
#include <cstdint>
#include <pthread.h>
#include <string>
#include <string_view>

namespace muduo20::CurrentThread {

extern thread_local int t_cachedTid;
extern thread_local std::array<char, 32> t_tidString;
extern thread_local int t_tidStringLength;
extern thread_local const char *t_threadName;

void cacheTid();
void setName(const char *name);

[[nodiscard]] inline int tid() {
    if (t_cachedTid == 0) [[unlikely]] {
        cacheTid();
    }
    return t_cachedTid;
}

[[nodiscard]] inline const char *tidString() { return t_tidString.data(); }
[[nodiscard]] inline std::string_view tidStringView() {
    return std::string_view{t_tidString.data(),
                            static_cast<size_t>(t_tidStringLength)};
}
[[nodiscard]] inline int tidStringLength() { return t_tidStringLength; }
[[nodiscard]] inline const char *name() { return t_threadName; }
[[nodiscard]] inline pthread_t pthreadId() noexcept { return ::pthread_self(); }

[[nodiscard]] bool isMainThread();
void sleepUsec(int64_t usec);

[[nodiscard]] std::string stackTrace(bool demangle);

} // namespace muduo20::CurrentThread
