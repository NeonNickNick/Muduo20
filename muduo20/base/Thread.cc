#include "muduo20/base/Thread.h"

#include "muduo20/base/CurrentThread.h"
#include "muduo20/base/Exception.h"
#include "muduo20/base/Print.h"

#include <atomic>
#include <cstdlib>
#include <exception>
#include <linux/prctl.h>
#include <sched.h>
#include <stop_token>
#include <sys/prctl.h>

#include <cassert>
#include <format>
#include <sys/types.h>
#include <thread>

using namespace muduo20;

std::atomic<int> Thread::numCreated_{0};

void Thread::setDefaultName() {
    const int num = ++numCreated_;
    if (name_.empty()) {
        name_ = std::format("Thread{}", num);
    }
}

void Thread::runInThread() {
    tid_.store(static_cast<pid_t>(CurrentThread::tid()),
               std::memory_order_release);
    tid_.notify_one();

    CurrentThread::setName(name_.empty() ? "muduo20Thread" : name_.c_str());
    const char *threadName = CurrentThread::name();
    ::prctl(PR_SET_NAME, threadName == nullptr ? "unknown" : threadName);

    try {
        func_();
        CurrentThread::setName("finished");
    } catch (const Exception &ex) {
        CurrentThread::setName("crashed");
        muduo20::io::eprintln("exception caught in Thread {}", name_);
        muduo20::io::eprintln("reason: {}", ex.what());
        muduo20::io::eprintln("stack trace: {}", ex.stackTrace());
        std::abort();
    } catch (const std::exception &ex) {
        CurrentThread::setName("crashed");
        muduo20::io::eprintln("exception caught in Thread {}", name_);
        muduo20::io::eprintln("reason: {}", ex.what());
        std::abort();
    } catch (...) {
        CurrentThread::setName("crashed");
        muduo20::io::eprintln("unknown exception caught in Thread {}", name_);
        throw;
    }
}

void Thread::start() {
    assert(!started());

    thread_ = std::jthread([this](const std::stop_token &) { runInThread(); });
    while (tid_.load(std::memory_order_acquire) == 0) {
        tid_.wait(0, std::memory_order_relaxed);
    }
    assert(tid_.load(std::memory_order_acquire) > 0);
}

int Thread::join() {
    assert(thread_.joinable());
    if (thread_.joinable()) {
        thread_.join();
    }
    return 0;
}
