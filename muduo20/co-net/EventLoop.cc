#include "muduo20/co-net/EventLoop.h"

#include "muduo20/base/CurrentThread.h"
#include "muduo20/base/Logging.h"
#include "muduo20/base/Timestamp.h"
#include "muduo20/base/noncopyable.h"
#include "muduo20/co-net/Callbacks.h"
#include "muduo20/co-net/Channel.h"
#include "muduo20/co-net/Poller.h"
#include "muduo20/co-net/SocketsOps.h"
#include "muduo20/co-net/TimerId.h"
#include "muduo20/co-net/TimerQueue.h"

#include <algorithm>
#include <atomic>
#include <cassert>
#include <chrono>
#include <csignal>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <signal.h>
#include <span>
#include <sys/eventfd.h>
#include <sys/types.h>
#include <unistd.h>
#include <utility>
#include <vector>

namespace muduo20::co_net {

namespace {

thread_local EventLoop *t_loopInThisThread = nullptr;
constexpr int kPollTimeMs = 10'000;

[[nodiscard]] int createEventfd() {
    const int eventFd = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (eventFd < 0) {
        muduo20::logSysFatal("EventLoop createEventfd failed");
    }
    return eventFd;
}

class IgnoreSigPipe : muduo20::noncopyable {
  public:
    IgnoreSigPipe() { ::signal(SIGPIPE, SIG_IGN); }
};

const IgnoreSigPipe g_ignoreSigPipe;

} // namespace

EventLoop *EventLoop::getEventLoopOfCurrentThread() noexcept {
    return t_loopInThisThread;
}

EventLoop::EventLoop()
    : threadId_(muduo20::CurrentThread::tid()),
      poller_(Poller::newDefaultPoller(this)),
      timerQueue_(std::make_unique<TimerQueue>(this)),
      wakeupFd_(createEventfd()),
      wakeupChannel_(std::make_unique<Channel>(this, wakeupFd_)) {
    muduo20::logDebug("EventLoop created {} in thread {}",
                      static_cast<const void *>(this), threadId_);
    if (t_loopInThisThread != nullptr) {
        muduo20::logFatal("Another EventLoop {} exists in this thread {}",
                          static_cast<const void *>(t_loopInThisThread),
                          threadId_);
    } else {
        t_loopInThisThread = this;
    }

    wakeupChannel_->setReadCallback(
        [this](Timestamp receiveTime) { handleRead(receiveTime); });
    wakeupChannel_->enableReading();
}

EventLoop::~EventLoop() {
    muduo20::logDebug("EventLoop {} of thread {} destructs in thread {}",
                      static_cast<const void *>(this), threadId_,
                      muduo20::CurrentThread::tid());
    wakeupChannel_->disableAll();
    wakeupChannel_->remove();
    ::close(wakeupFd_);
    t_loopInThisThread = nullptr;
}

void EventLoop::loop() {
    assert(!looping_.load(std::memory_order_relaxed));
    assertInLoopThread();

    looping_.store(true, std::memory_order_release);
    quit_.store(false, std::memory_order_release);
    muduo20::logTrace("EventLoop {} start looping",
                      static_cast<const void *>(this));

    while (!quit_.load(std::memory_order_acquire)) {
        activeChannels_.clear();
        pollReturnTime_ = poller_->poll(kPollTimeMs, &activeChannels_);
        ++iteration_;

        eventHandling_ = true;
        for (auto *channel : activeChannels_) {
            currentActiveChannel_ = channel;
            currentActiveChannel_->handleEvent(pollReturnTime_);
        }
        currentActiveChannel_ = nullptr;
        eventHandling_ = false;
        doPendingFunctors();
    }

    muduo20::logTrace("EventLoop {} stop looping",
                      static_cast<const void *>(this));
    looping_.store(false, std::memory_order_release);
}

void EventLoop::quit() {
    quit_.store(true, std::memory_order_release);
    if (!isInLoopThread()) {
        wakeup();
    }
}

void EventLoop::runInLoop(Functor cb) {
    if (isInLoopThread()) {
        cb();
        return;
    }
    queueInLoop(std::move(cb));
}

void EventLoop::queueInLoop(Functor cb) {
    {
        std::scoped_lock lock(mutex_);
        pendingFunctors_.push_back(std::move(cb));
    }

    if (!isInLoopThread() || callingPendingFunctors_) {
        wakeup();
    }
}

size_t EventLoop::queueSize() const {
    std::scoped_lock lock(mutex_);
    return pendingFunctors_.size();
}

TimerId EventLoop::runAt(Timestamp time, TimerCallback cb) {
    return timerQueue_->addTimer(std::move(cb), time,
                                 std::chrono::microseconds::zero());
}

#if MUDUO20_ENABLE_LEGACY_COMPAT
TimerId EventLoop::runAfter(double delaySeconds, TimerCallback cb) {
    const auto delay = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::duration<double>(delaySeconds));
    return runAfter(delay, std::move(cb));
}
#endif

TimerId EventLoop::runAfter(std::chrono::microseconds delay, TimerCallback cb) {
    const Timestamp::TimePoint when = Timestamp::now().timePoint() + delay;
    return runAt(Timestamp{when}, std::move(cb));
}

#if MUDUO20_ENABLE_LEGACY_COMPAT
TimerId EventLoop::runEvery(double intervalSeconds, TimerCallback cb) {
    const auto interval = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::duration<double>(intervalSeconds));
    return runEvery(interval, std::move(cb));
}
#endif

TimerId EventLoop::runEvery(std::chrono::microseconds interval,
                            TimerCallback cb) {
    const Timestamp::TimePoint when = Timestamp::now().timePoint() + interval;
    return timerQueue_->addTimer(std::move(cb), Timestamp{when}, interval);
}

void EventLoop::cancel(TimerId timerId) { timerQueue_->cancel(timerId); }

void EventLoop::updateChannel(Channel *channel) {
    assert(channel->ownerLoop() == this);
    assertInLoopThread();
    poller_->updateChannel(channel);
}

void EventLoop::removeChannel(Channel *channel) {
    assert(channel->ownerLoop() == this);
    assertInLoopThread();
    if (eventHandling_) {
        assert(currentActiveChannel_ == channel ||
               std::find(activeChannels_.begin(), activeChannels_.end(),
                         channel) == activeChannels_.end());
    }
    poller_->removeChannel(channel);
}

bool EventLoop::hasChannel(Channel *channel) const {
    assert(channel->ownerLoop() == this);
    assertInLoopThread();
    return poller_->hasChannel(channel);
}

void EventLoop::abortNotInLoopThread() const {
    muduo20::logFatal(
        "EventLoop::abortNotInLoopThread - EventLoop {} was created in "
        "threadId_ = {}, current thread id = {}",
        static_cast<const void *>(this), threadId_,
        muduo20::CurrentThread::tid());
}

void EventLoop::assertInLoopThread() const {
    if (!isInLoopThread()) {
        abortNotInLoopThread();
    }
}

bool EventLoop::isInLoopThread() const {
    return threadId_ == muduo20::CurrentThread::tid();
}

void EventLoop::wakeup() const {
    const std::uint64_t one = 1;
    const auto bytes = std::as_bytes(std::span{&one, 1});
    const ssize_t written = sockets::write(wakeupFd_, bytes);
    if (written != static_cast<ssize_t>(sizeof(one))) {
        muduo20::logError("EventLoop::wakeup() writes {} bytes instead of 8",
                          written);
    }
}

void EventLoop::handleRead([[maybe_unused]] Timestamp receiveTime) const {
    std::uint64_t one = 0;
    const auto bytes = std::as_writable_bytes(std::span{&one, 1});
    const ssize_t n = sockets::read(wakeupFd_, bytes);
    if (n != static_cast<ssize_t>(sizeof(one))) {
        muduo20::logError("EventLoop::handleRead() reads {} bytes instead of 8",
                          n);
    }
}

void EventLoop::doPendingFunctors() {
    std::vector<Functor> functors;
    callingPendingFunctors_ = true;

    {
        std::scoped_lock lock(mutex_);
        functors.swap(pendingFunctors_);
    }

    for (auto &functor : functors) {
        functor();
    }
    callingPendingFunctors_ = false;
}

} // namespace muduo20::co_net
