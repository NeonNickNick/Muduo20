#include "muduo20/base/Logging.h"

#include <benchmark/benchmark.h>
#include <atomic>

namespace {

std::atomic<int64_t> g_totalBytes{0};

void dummyOutput(const char *msg, int len) {
  (void)msg;
  g_totalBytes.fetch_add(len, std::memory_order_relaxed);
}

void dummyFlush() {}

void benchSetup() {
  g_totalBytes.store(0, std::memory_order_release);
  muduo20::Logger::setOutput(dummyOutput);
  muduo20::Logger::setFlush(dummyFlush);
  muduo20::Logger::setLogLevel(muduo20::Logger::LogLevel::INFO);
}

static void BM_Logging_Format(benchmark::State &state) {
  benchSetup();
  int i = 0;
  for (auto _ : state) {
    (void)_;
    muduo20::logInfo("Hello 0123456789 abcdefghijklmnopqrstuvwxyz {}", i);
    ++i;
  }
  const auto bytes = static_cast<double>(g_totalBytes.load(std::memory_order_acquire));
  state.SetItemsProcessed(static_cast<int64_t>(state.iterations()));
  state.SetBytesProcessed(static_cast<int64_t>(bytes));
  state.counters["bytes_per_msg"] = bytes / static_cast<double>(state.iterations());
}

static void BM_Logging_Stream(benchmark::State &state) {
  benchSetup();
  int i = 0;
  for (auto _ : state) {
    (void)_;
    muduo20::logInfo() << "Hello 0123456789 abcdefghijklmnopqrstuvwxyz " << i;
    ++i;
  }
  const auto bytes =
      static_cast<double>(g_totalBytes.load(std::memory_order_acquire));
  state.SetItemsProcessed(static_cast<int64_t>(state.iterations()));
  state.SetBytesProcessed(static_cast<int64_t>(bytes));
  state.counters["bytes_per_msg"] = bytes / static_cast<double>(state.iterations());
}

static void BM_Logging_FormatConst(benchmark::State &state) {
  benchSetup();
  for (auto _ : state) {
    (void)_;
    muduo20::logInfo("Hello 0123456789 abcdefghijklmnopqrstuvwxyz");
  }
  const auto bytes =
      static_cast<double>(g_totalBytes.load(std::memory_order_acquire));
  state.SetItemsProcessed(static_cast<int64_t>(state.iterations()));
  state.SetBytesProcessed(static_cast<int64_t>(bytes));
  state.counters["bytes_per_msg"] = bytes / static_cast<double>(state.iterations());
}

} // namespace

BENCHMARK(BM_Logging_Format)->UseRealTime()->Threads(1);
BENCHMARK(BM_Logging_Stream)->UseRealTime()->Threads(1);
BENCHMARK(BM_Logging_FormatConst)->UseRealTime()->Threads(1);
