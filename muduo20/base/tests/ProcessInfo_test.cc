#include "muduo20/base/CurrentThread.h"
#include "muduo20/base/ProcessInfo.h"

#include <gtest/gtest.h>

#include <algorithm>

TEST(ProcessInfo, BasicProcessProperties) {
  EXPECT_GT(muduo20::ProcessInfo::pid(), 0);
  EXPECT_FALSE(muduo20::ProcessInfo::pidString().empty());
  EXPECT_FALSE(muduo20::ProcessInfo::hostname().empty());
  EXPECT_FALSE(muduo20::ProcessInfo::procStat().empty());
  EXPECT_FALSE(muduo20::ProcessInfo::procStatus().empty());
  EXPECT_FALSE(muduo20::ProcessInfo::threadStat().empty());
  EXPECT_FALSE(muduo20::ProcessInfo::exePath().empty());
  EXPECT_TRUE(muduo20::ProcessInfo::startTime().valid());
}

TEST(ProcessInfo, ThreadsAndProcName) {
  const auto stat = muduo20::ProcessInfo::procStat();
  EXPECT_FALSE(muduo20::ProcessInfo::procname(stat).empty());

  const auto tids = muduo20::ProcessInfo::threads();
  EXPECT_GE(static_cast<int>(tids.size()), 1);
  EXPECT_GE(muduo20::ProcessInfo::numThreads(), 1);
  EXPECT_TRUE(std::binary_search(tids.begin(), tids.end(),
                                 muduo20::CurrentThread::tid()));
}

TEST(ProcessInfo, ResourcesAndCpuTime) {
  EXPECT_GT(muduo20::ProcessInfo::openedFiles(), 0);
  EXPECT_GE(muduo20::ProcessInfo::maxOpenFiles(), muduo20::ProcessInfo::openedFiles());
  EXPECT_GT(muduo20::ProcessInfo::clockTicksPerSecond(), 0);
  EXPECT_GT(muduo20::ProcessInfo::pageSize(), 0);

  const auto cpu = muduo20::ProcessInfo::cpuTime();
  EXPECT_GE(cpu.userSeconds, 0.0);
  EXPECT_GE(cpu.systemSeconds, 0.0);
  EXPECT_GE(cpu.total(), 0.0);
}
