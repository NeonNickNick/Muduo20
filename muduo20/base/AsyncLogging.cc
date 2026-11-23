#include "muduo20/base/AsyncLogging.h"

#include "muduo20/base/CurrentThread.h"
#include "muduo20/base/LogFile.h"
#include "muduo20/base/Print.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <format>
#include <iterator>
#include <memory>
#include <mutex>
#include <ranges>
#include <stop_token>
#include <string>
#include <string_view>
#include <thread>
#include <utility>

using namespace muduo20;

namespace {

size_t computeDefaultShardCount() {
    const auto hw = std::thread::hardware_concurrency();
    const size_t half = hw == 0 ? 4U : static_cast<size_t>(hw / 2U);
    const size_t bounded = std::min<size_t>(8, std::max<size_t>(2, half));
    return std::bit_ceil(bounded);
}

size_t normalizeShardCount(size_t requested) {
    if (requested == 0) {
        return computeDefaultShardCount();
    }
    const size_t bounded = std::max<size_t>(2, requested);
    return std::bit_ceil(bounded);
}

} // namespace

AsyncLogging::AsyncLogging(std::string basename, std::int64_t rollSize,
                           int flushInterval, size_t shardCount)
    : flushInterval_(flushInterval), basename_(std::move(basename)),
      rollSize_(rollSize), shardCount_(normalizeShardCount(shardCount)),
      shardMask_(shardCount_ - 1), shards_(shardCount_) {
    for (auto &shard : shards_) {
        shard.currentBuffer_ = std::make_unique<Buffer>();
        shard.nextBuffer_ = std::make_unique<Buffer>();
        shard.buffers_.reserve(8);
    }
    globalPool_.reserve(kGlobalPoolMax);
    for (size_t i = 0; i < 16; ++i) {
        globalPool_.emplace_back(std::make_unique<Buffer>());
    }
}

AsyncLogging::BufferPtr AsyncLogging::acquireBuffer() {
    std::scoped_lock lock(poolMutex_);
    if (!globalPool_.empty()) {
        BufferPtr buf = std::move(globalPool_.back());
        globalPool_.pop_back();
        (*buf).reset();
        return buf;
    }
    return std::make_unique<Buffer>();
}

void AsyncLogging::recycleBuffer(BufferPtr buffer) {
    if (!buffer) {
        return;
    }
    (*buffer).reset();
    std::scoped_lock lock(poolMutex_);
    if (globalPool_.size() < kGlobalPoolMax) {
        globalPool_.emplace_back(std::move(buffer));
    }
}

AsyncLogging::~AsyncLogging() {
    if (started_.load(std::memory_order_acquire)) {
        stop();
    }
}

size_t AsyncLogging::shardIndex() const noexcept {
    thread_local size_t cached =
        static_cast<size_t>(muduo20::CurrentThread::tid()) & shardMask_;
    return cached;
}

#if MUDUO20_ENABLE_LEGACY_COMPAT
void AsyncLogging::append(const char *logline, int len) {
    append(std::string_view{logline, static_cast<size_t>(len)});
}
#endif

void AsyncLogging::append(std::string_view logline) {
    auto &shard = shards_.at(shardIndex());
    bool needWakeup = false;
    {
        std::scoped_lock lock(shard.mutex_);
        if (shard.currentBuffer_->avail() >= static_cast<int>(logline.size())) {
            shard.currentBuffer_->append(logline);
            return;
        }

        shard.buffers_.emplace_back(std::move(shard.currentBuffer_));
        if (shard.nextBuffer_) {
            shard.currentBuffer_ = std::move(shard.nextBuffer_);
        } else {
            shard.currentBuffer_ = acquireBuffer();
        }
        shard.currentBuffer_->append(logline);
        needWakeup = true;
    }

    if (needWakeup) {
        wakeup_.release();
    }
}

void AsyncLogging::start() {
    if (bool expected = false; !started_.compare_exchange_strong(
            expected, true, std::memory_order_release,
            std::memory_order_relaxed)) {
        return;
    }

    thread_ =
        std::jthread([this](std::stop_token st) { threadFunc(std::move(st)); });
    startedSignal_.acquire();
}

void AsyncLogging::stop() {
    if (bool expected = true; !started_.compare_exchange_strong(
            expected, false, std::memory_order_acq_rel,
            std::memory_order_relaxed)) {
        return;
    }
    if (thread_.joinable()) {
        thread_.request_stop();
        thread_.join();
    }
}

void AsyncLogging::threadFunc(std::stop_token stopToken) {
    LogFile output(basename_, rollSize_, false);
    BufferVector buffersToWrite;
    buffersToWrite.reserve(64);
    BufferVector spareBuffers;
    spareBuffers.reserve(32);

    // Initialize spare buffers to avoid allocation at startup
    for (int i = 0; i < 16; ++i) {
        spareBuffers.emplace_back(std::make_unique<Buffer>());
    }

    auto collect = [&]() {
        for (auto &shard : shards_) {
            std::scoped_lock lock(shard.mutex_);

            // If current buffer has data, move it to shard.buffers_
            if (shard.currentBuffer_->length() > 0) {
                shard.buffers_.emplace_back(std::move(shard.currentBuffer_));

                if (shard.nextBuffer_) {
                    shard.currentBuffer_ = std::move(shard.nextBuffer_);
                } else if (!spareBuffers.empty()) {
                    shard.currentBuffer_ = std::move(spareBuffers.back());
                    spareBuffers.pop_back();
                    (*shard.currentBuffer_).reset();
                } else {
                    shard.currentBuffer_ = acquireBuffer();
                }
            }

            // Replenish nextBuffer if it's missing (Zero Allocation Principle)
            if (!shard.nextBuffer_) {
                if (!spareBuffers.empty()) {
                    shard.nextBuffer_ = std::move(spareBuffers.back());
                    spareBuffers.pop_back();
                    (*shard.nextBuffer_).reset();
                } else {
                    shard.nextBuffer_ = acquireBuffer();
                }
            }

            // Move accumulated buffers from shard to local write queue
            if (!shard.buffers_.empty()) {
                std::ranges::move(shard.buffers_,
                                  std::back_inserter(buffersToWrite));
                shard.buffers_.clear();
            }
        }
    };

    std::stop_callback wakeOnStop(stopToken, [this]() { wakeup_.release(); });
    startedSignal_.release();

    while (!stopToken.stop_requested()) {
        wakeup_.try_acquire_for(std::chrono::seconds(flushInterval_));

        // Collect buffers from all shards
        collect();

        if (buffersToWrite.empty()) {
            continue;
        }

        if (buffersToWrite.size() > 25) {
            const auto nowSec = std::chrono::floor<std::chrono::seconds>(
                std::chrono::system_clock::now());

            std::array<char, 256> buf{};
            const auto result =
                std::format_to_n(buf.data(), buf.size() - 1,
                                 "Dropped log messages at {:%Y%m%d %H:%M:%S}, "
                                 "{} larger buffers\n",
                                 nowSec, buffersToWrite.size() - 2);
            const size_t safeLen =
                std::min<size_t>(result.size, buf.size() - 1);
            std::string_view dropped(buf.data(), safeLen);
            muduo20::io::eprint(dropped);
            muduo20::io::eflush();
            output.append(dropped);
            buffersToWrite.erase(std::ranges::next(buffersToWrite.begin(), 2),
                                 buffersToWrite.end());
        }

        std::ranges::for_each(buffersToWrite,
                              [&output](const BufferPtr &buffer) {
                                  output.append(buffer->toStringView());
                              });

        output.flush();

        if (spareBuffers.size() < 32) {
            const size_t needed = 32 - spareBuffers.size();
            const size_t available = buffersToWrite.size();
            const size_t toMove = std::min(needed, available);

            auto src = std::ranges::subrange(buffersToWrite.begin(),
                                             buffersToWrite.begin() + toMove);
            std::ranges::move(src, std::back_inserter(spareBuffers));
        }
        buffersToWrite.clear();
    }

    collect();
    std::ranges::for_each(buffersToWrite, [&output](const BufferPtr &buffer) {
        output.append(buffer->toStringView());
    });

    output.flush();

    std::ranges::for_each(spareBuffers, [this](BufferPtr &buf) {
        recycleBuffer(std::move(buf));
    });
    std::ranges::for_each(buffersToWrite, [this](BufferPtr &buf) {
        recycleBuffer(std::move(buf));
    });
}
