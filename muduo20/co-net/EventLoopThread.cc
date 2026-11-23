#include "muduo20/co-net/EventLoopThread.h"

#include "muduo20/co-net/EventLoop.h"
#include <atomic>
#include <cassert>
#include <mutex>
#include <string>
#include <utility>

namespace muduo20::co_net {

EventLoopThread::EventLoopThread(ThreadInitCallback cb, std::string name)
    : thread_([this] { threadFunc(); }, std::move(name)),
      callback_(std::move(cb)) {}

EventLoopThread::~EventLoopThread() {
    exiting_.store(true, std::memory_order_release);
    EventLoop *loop = nullptr;
    {
        std::scoped_lock lock(mutex_);
        loop = loop_;
    }
    if (loop != nullptr) {
        loop->quit();
        thread_.join();
    }
}

EventLoop *EventLoopThread::startLoop() {
    assert(!thread_.started());
    thread_.start();

    std::unique_lock lock(mutex_);
    cond_.wait(lock, [this] { return loop_ != nullptr; });
    return loop_;
}

void EventLoopThread::threadFunc() {
    EventLoop loop;

    if (callback_) {
        callback_(&loop);
    }

    {
        std::scoped_lock lock(mutex_);
        loop_ = &loop;
        cond_.notify_one();
    }

    loop.loop();

    std::scoped_lock lock(mutex_);
    loop_ = nullptr;
}

} // namespace muduo20::co_net
