#include "muduo20/base/CurrentThread.h"

#include <gtest/gtest.h>

#include <chrono>
#include <future>
#include <pthread.h>
#include <string>
#include <thread>

TEST(CurrentThread, BasicPropertiesInMainThread) {
    EXPECT_GT(muduo20::CurrentThread::tid(), 0);
    EXPECT_TRUE(muduo20::CurrentThread::isMainThread());
    EXPECT_GT(muduo20::CurrentThread::tidStringLength(), 0);
    EXPECT_EQ(muduo20::CurrentThread::tidStringView().size(),
              static_cast<size_t>(muduo20::CurrentThread::tidStringLength()));
}

TEST(CurrentThread, NameAndMainThreadFlagInWorker) {
    std::promise<bool> isMainPromise;
    std::promise<std::string> namePromise;

    std::jthread worker([&](std::stop_token) {
        muduo20::CurrentThread::setName("worker-test");
        isMainPromise.set_value(muduo20::CurrentThread::isMainThread());
        namePromise.set_value(std::string(muduo20::CurrentThread::name()));
    });

    EXPECT_FALSE(isMainPromise.get_future().get());
    EXPECT_EQ(namePromise.get_future().get(), "worker-test");
}

TEST(CurrentThread, SleepUsecAndStackTrace) {
    using namespace std::chrono_literals;
    const auto before = std::chrono::steady_clock::now();
    muduo20::CurrentThread::sleepUsec(5'000);
    const auto elapsed = std::chrono::steady_clock::now() - before;
    EXPECT_GE(elapsed, 4ms);

    const auto trace = muduo20::CurrentThread::stackTrace(false);
    EXPECT_FALSE(trace.empty());
}

TEST(CurrentThread, PthreadIdMatchesSelf) {
    EXPECT_NE(muduo20::CurrentThread::pthreadId(), static_cast<pthread_t>(0));
    EXPECT_TRUE(pthread_equal(muduo20::CurrentThread::pthreadId(),
                              ::pthread_self()) != 0);
}
