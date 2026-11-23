#pragma once

#include "muduo20/base/coroutine/Executor.h"
#include "muduo20/base/coroutine/Task.h"
#include "muduo20/co-net/EventLoop.h"

#include <functional>
#include <utility>

namespace muduo20::co_net {

// Thin adapter: coroutine primitives know only Executor; this header is the
// only bridge to the co-net EventLoop.
class EventLoopExecutor {
public:
  explicit EventLoopExecutor(EventLoop &loop)
      : executor_([&loop](muduo20::coroutine::Executor::Function fn) {
          loop.queueInLoop(std::move(fn));
        }) {}

  void start(muduo20::coroutine::Task<void> &task) const {
    executor_.start(task);
  }

  [[nodiscard]] muduo20::coroutine::Executor &executor() noexcept {
    return executor_;
  }

private:
  muduo20::coroutine::Executor executor_;
};

} // namespace muduo20::co_net
