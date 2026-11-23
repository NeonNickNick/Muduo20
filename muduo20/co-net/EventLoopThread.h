#pragma once

#include "muduo20/base/Thread.h"
#include "muduo20/base/noncopyable.h"
#include "muduo20/co-net/Callbacks.h"

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <string>
#include <utility>

namespace muduo20::co_net {

class EventLoop;

class EventLoopThread : muduo20::noncopyable {
  public:
    using ThreadInitCallback = CallbackFunction<void(EventLoop *)>;

    explicit EventLoopThread(ThreadInitCallback cb = {}, std::string name = {});
    template <typename F>
        requires CallbackBindable<F, ThreadInitCallback>
    explicit EventLoopThread(F &&cb, std::string name = {})
        : EventLoopThread(ThreadInitCallback(std::forward<F>(cb)),
                          std::move(name)) {}
    ~EventLoopThread();

    [[nodiscard]] EventLoop *startLoop();

  private:
    void threadFunc();

    EventLoop *loop_{nullptr};
    std::atomic<bool> exiting_{false};
    muduo20::Thread thread_;
    std::mutex mutex_;
    std::condition_variable cond_;
    ThreadInitCallback callback_;
};

} // namespace muduo20::co_net
