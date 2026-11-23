#pragma once

#include "muduo20/base/noncopyable.h"
#include "muduo20/co-net/Callbacks.h"

#include <cstddef>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace muduo20::co_net {

class EventLoop;
class EventLoopThread;

class EventLoopThreadPool : muduo20::noncopyable {
  public:
    using ThreadInitCallback = CallbackFunction<void(EventLoop *)>;

    EventLoopThreadPool(EventLoop *baseLoop, std::string nameArg);
    ~EventLoopThreadPool();

    void setThreadNum(int numThreads) { numThreads_ = numThreads; }
    void start(ThreadInitCallback cb = {});
    template <typename F>
        requires CallbackBindable<F, ThreadInitCallback>
    void start(F &&cb) {
        start(ThreadInitCallback(std::forward<F>(cb)));
    }

    [[nodiscard]] EventLoop *getNextLoop();
    [[nodiscard]] EventLoop *getLoopForHash(size_t hashCode);
    [[nodiscard]] std::vector<EventLoop *> getAllLoops();

    [[nodiscard]] bool started() const noexcept { return started_; }
    [[nodiscard]] const std::string &name() const noexcept { return name_; }

  private:
    EventLoop *baseLoop_;
    std::string name_;
    bool started_{false};
    int numThreads_{0};
    int next_{0};
    std::vector<std::unique_ptr<EventLoopThread>> threads_;
    std::vector<EventLoop *> loops_;
};

} // namespace muduo20::co_net
