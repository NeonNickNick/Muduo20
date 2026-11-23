#pragma once

#include "Executor.h"

#include <coroutine>
#include <cstddef>
#include <exception>
#include <functional>
#include <memory>
#include <mutex>
#include <stop_token>
#include <system_error>
#include <utility>

namespace muduo20::coroutine {

struct IoResult {
  std::error_code error;
  std::size_t bytes{0};
  [[nodiscard]] explicit operator bool() const noexcept { return !error; }
};

// Bridges an existing callback-based async operation to co_await. The starter
// receives a completion callback and returns a function that cancels the
// underlying operation. Completion and cancellation are first-wins.
class OperationAwaiter {
public:
  using Completion = std::function<void(IoResult)>;
  using Cancel = std::function<void()>;
  using Starter = std::function<Cancel(Completion)>;

  OperationAwaiter(Executor &executor, std::stop_token token, Starter starter)
      : state_(std::make_shared<State>(executor)), starter_(std::move(starter)),
        token_(token) {}
  OperationAwaiter(OperationAwaiter &&other) noexcept
      : state_(std::move(other.state_)), starter_(std::move(other.starter_)),
        token_(std::move(other.token_)), stopCallback_(std::move(other.stopCallback_)) {}
  OperationAwaiter(const OperationAwaiter &) = delete;
  OperationAwaiter &operator=(const OperationAwaiter &) = delete;
  ~OperationAwaiter() { if (state_) state_->abandon(); }

  [[nodiscard]] bool await_ready() const noexcept { return false; }

  void await_suspend(std::coroutine_handle<> continuation) {
    state_->setContinuation(continuation);
    std::weak_ptr<State> weak = state_;
    stopCallback_ = std::make_unique<std::stop_callback<std::function<void()>>>(
        token_, [weak] {
          if (auto state = weak.lock()) {
            state->cancel();
          }
        });

    auto completion = [weak](IoResult result) {
      if (auto state = weak.lock()) {
        state->complete(result);
      }
    };
    auto cancel = starter_(std::move(completion));
    state_->setCancel(std::move(cancel));
    if (token_.stop_requested()) {
      state_->cancel();
    }
  }

  [[nodiscard]] IoResult await_resume() const { return state_->result(); }

private:
  struct State : std::enable_shared_from_this<State> {
    explicit State(Executor &executor) : executor(&executor) {}
    void setContinuation(std::coroutine_handle<> h) {
      std::lock_guard lock(mutex);
      continuation = h;
    }
    void setCancel(Cancel fn) {
      bool cancelNow = false;
      {
        std::lock_guard lock(mutex);
        cancelFunction = std::move(fn);
        cancelNow = cancelled && !completed;
      }
      if (cancelNow) invokeCancel();
    }
    void complete(IoResult value) {
      std::coroutine_handle<> h;
      {
        std::lock_guard lock(mutex);
        if (completed || abandoned) return;
        completed = true;
        output = value;
        h = continuation;
      }
      auto weak = this->weak_from_this();
      executor->post([weak, h] {
        if (auto state = weak.lock()) {
          {
            std::lock_guard lock(state->mutex);
            if (state->abandoned) return;
          }
          if (h && !h.done()) h.resume();
        }
      });
    }
    void cancel() {
      bool shouldInvoke = false;
      {
        std::lock_guard lock(mutex);
        if (completed || abandoned) return;
        cancelled = true;
        shouldInvoke = static_cast<bool>(cancelFunction);
      }
      if (shouldInvoke) invokeCancel();
      complete({std::make_error_code(std::errc::operation_canceled), 0});
    }
    void invokeCancel() {
      Cancel fn;
      {
        std::lock_guard lock(mutex);
        fn = cancelFunction;
      }
      if (fn) fn();
    }
    IoResult result() const {
      std::lock_guard lock(mutex);
      return output;
    }
    void abandon() {
      bool shouldCancel = false;
      {
        std::lock_guard lock(mutex);
        if (!completed) {
          abandoned = true;
          shouldCancel = static_cast<bool>(cancelFunction);
        }
      }
      if (shouldCancel) invokeCancel();
    }

    Executor *executor;
    mutable std::mutex mutex;
    std::coroutine_handle<> continuation{};
    Cancel cancelFunction;
    IoResult output{};
    bool completed{false};
    bool cancelled{false};
    bool abandoned{false};
  };

  std::shared_ptr<State> state_;
  Starter starter_;
  std::stop_token token_;
  std::unique_ptr<std::stop_callback<std::function<void()>>> stopCallback_;
};

} // namespace muduo20::coroutine
