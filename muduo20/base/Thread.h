#pragma once

#include "muduo20/base/Types.h"
#include "muduo20/base/noncopyable.h"

#include <atomic>
#include <concepts>
#include <sched.h>
#include <string>
#include <sys/types.h>
#include <thread>
#include <type_traits>
#include <utility>

namespace muduo20 {

class Thread : noncopyable {
  public:
#if MUDUO20_ENABLE_LEGACY_COMPAT
    using ThreadFunc = CallbackFunction<void()>;
#else
    using ThreadFunc = detail::MoveOnlyFunction<void()>;
#endif

    template <typename F>
        requires(!std::same_as<std::remove_cvref_t<F>, Thread> &&
                 std::constructible_from<ThreadFunc, F>)
    explicit Thread(F &&func, std::string name = std::string{})
        : func_(std::forward<F>(func)), name_(std::move(name)) {
        setDefaultName();
    }

    void start();
    int join();

    [[nodiscard]] bool started() const {
        return tid_.load(std::memory_order_acquire) > 0;
    }
    [[nodiscard]] bool joined() const {
        return started() && !thread_.joinable();
    }
    [[nodiscard]] pid_t tid() const {
        return tid_.load(std::memory_order_acquire);
    }
    [[nodiscard]] const std::string &name() const { return name_; }

    [[nodiscard]] static int numCreated() { return numCreated_.load(); }

  private:
    void setDefaultName();
    void runInThread();

    std::atomic<pid_t> tid_{0};
    ThreadFunc func_;
    std::string name_;
    std::jthread thread_;

    static std::atomic<int> numCreated_;
};

} // namespace muduo20
