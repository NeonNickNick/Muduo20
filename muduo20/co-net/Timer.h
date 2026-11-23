#pragma once

#include "muduo20/base/Timestamp.h"
#include "muduo20/base/noncopyable.h"
#include "muduo20/co-net/Callbacks.h"

#include <atomic>
#include <chrono>
#include <cstdint>

namespace muduo20::co_net {

class Timer : muduo20::noncopyable {
public:
  Timer(TimerCallback cb, Timestamp when, std::chrono::microseconds interval)
      : callback_(std::move(cb)), expiration_(when), interval_(interval),
        repeat_(interval > std::chrono::microseconds::zero()),
        sequence_(s_numCreated_.fetch_add(1, std::memory_order_relaxed) + 1) {}

  void run() { callback_(); }

  [[nodiscard]] Timestamp expiration() const { return expiration_; }
  [[nodiscard]] bool repeat() const { return repeat_; }
  [[nodiscard]] std::int64_t sequence() const { return sequence_; }

  void restart(Timestamp now);

  [[nodiscard]] static std::int64_t numCreated() {
    return s_numCreated_.load(std::memory_order_relaxed);
  }

private:
  TimerCallback callback_;
  Timestamp expiration_;
  const std::chrono::microseconds interval_;
  const bool repeat_;
  const std::int64_t sequence_;

  static std::atomic<std::int64_t> s_numCreated_;
};

} // namespace muduo20::co_net
