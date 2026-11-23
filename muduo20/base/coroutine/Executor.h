#pragma once

#include <coroutine>
#include <functional>
#include <utility>

#include "Task.h"

namespace muduo20::coroutine {

// Scheduler-independent executor contract used by coroutine primitives.
class Executor {
public:
  using Function = std::function<void()>;
  using Scheduler = std::function<void(Function)>;

  explicit Executor(Scheduler scheduler) : scheduler_(std::move(scheduler)) {}
  void post(Function function) const { scheduler_(std::move(function)); }
  void resume(std::coroutine_handle<> handle) const {
    post([handle] { if (handle && !handle.done()) handle.resume(); });
  }
  // The caller retains ownership of task until it reaches done().
  void start(Task<void> &task) const { resume(task.handle()); }

private:
  Scheduler scheduler_;
};

} // namespace muduo20::coroutine
