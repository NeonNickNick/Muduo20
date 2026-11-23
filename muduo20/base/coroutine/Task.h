#pragma once

#include <concepts>
#include <coroutine>
#include <exception>
#include <optional>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace muduo20::coroutine {

// Lazy, move-only coroutine task. It is scheduler agnostic: an asynchronous
// awaiter may arrange for the suspended coroutine to be resumed by its
// executor.
template <typename T = void> class [[nodiscard]] Task {
    static_assert(!std::is_reference_v<T>, "Task<T&> is not supported");

  public:
    struct promise_type;
    using handle_type = std::coroutine_handle<promise_type>;

    class Awaiter {
      public:
        explicit Awaiter(handle_type h) noexcept : handle_(h) {}
        Awaiter(Awaiter &&other) noexcept
            : handle_(std::exchange(other.handle_, {})) {}
        Awaiter(const Awaiter &) = delete;
        Awaiter &operator=(const Awaiter &) = delete;
        ~Awaiter() {
            if (handle_) {
                handle_.destroy();
            }
        }
        [[nodiscard]] bool await_ready() const noexcept {
            return handle_ && handle_.done();
        }
        std::coroutine_handle<>
        await_suspend(std::coroutine_handle<> parent) noexcept {
            if (!handle_) {
                return std::noop_coroutine();
            }
            handle_.promise().continuation = parent;
            return handle_;
        }
        T await_resume() {
            if (!handle_) {
                throw std::logic_error("awaiting an empty Task");
            }
            auto &p = handle_.promise();
            if (p.exception) {
                std::rethrow_exception(p.exception);
            }
            return std::move(*p.value);
        }

      private:
        handle_type handle_;
    };

    struct promise_type {
        std::optional<T> value_;
        std::exception_ptr exception_;
        std::coroutine_handle<> continuation_ = std::noop_coroutine();
        Task get_return_object() noexcept {
            return Task(handle_type::from_promise(*this));
        }
        [[nodiscard]] std::suspend_always initial_suspend() const noexcept {
            return {};
        }
        struct FinalAwaiter {
            [[nodiscard]] bool await_ready() const noexcept { return false; }
            std::coroutine_handle<>
            await_suspend(handle_type h) const noexcept {
                return h.promise().continuation;
            }
            void await_resume() const noexcept {}
        };
        FinalAwaiter final_suspend() const noexcept { return {}; }
        template <typename U>
            requires std::constructible_from<T, U &&>
        void
        return_value(U &&v) noexcept(std::is_nothrow_constructible_v<T, U &&>) {
            value_.emplace(std::forward<U>(v));
        }
        void unhandled_exception() noexcept {
            exception_ = std::current_exception();
        }
    };

    Task() noexcept = default;
    explicit Task(handle_type h) noexcept : handle_(h) {}
    Task(Task &&other) noexcept : handle_(std::exchange(other.handle_, {})) {}
    Task &operator=(Task &&other) noexcept {
        if (this != &other) {
            if (handle_) {
                handle_.destroy();
            }
            handle_ = std::exchange(other.handle_, {});
        }
        return *this;
    }
    Task(const Task &) = delete;
    Task &operator=(const Task &) = delete;
    ~Task() {
        if (handle_) {
            handle_.destroy();
        }
    }
    // Resume only when an external scheduler knows the task is ready.
    void resume() const {
        if (handle_ && !handle_.done()) {
            handle_.resume();
        }
    }
    [[nodiscard]] bool done() const noexcept {
        return handle_ && handle_.done();
    }
    [[nodiscard]] explicit operator bool() const noexcept { return !!handle_; }
    [[nodiscard]] handle_type handle() const noexcept { return handle_; }
    [[nodiscard]] Awaiter operator co_await() && {
        if (!handle_) {
            throw std::logic_error("awaiting an empty Task");
        }
        return Awaiter(std::exchange(handle_, {}));
    }

  private:
    handle_type handle_{};
};

template <> class [[nodiscard]] Task<void> {
  public:
    struct promise_type;
    using handle_type = std::coroutine_handle<promise_type>;
    class Awaiter {
      public:
        explicit Awaiter(handle_type h) noexcept : handle_(h) {}
        Awaiter(Awaiter &&other) noexcept
            : handle_(std::exchange(other.handle_, {})) {}
        Awaiter(const Awaiter &) = delete;
        Awaiter &operator=(const Awaiter &) = delete;
        ~Awaiter() {
            if (handle_)
                handle_.destroy();
        }
        [[nodiscard]] bool await_ready() const noexcept {
            return handle_ && handle_.done();
        }
        std::coroutine_handle<>
        await_suspend(std::coroutine_handle<> parent) noexcept {
            if (!handle_)
                return std::noop_coroutine();
            handle_.promise().continuation = parent;
            return handle_;
        }
        void await_resume() {
            if (!handle_)
                throw std::logic_error("awaiting an empty Task");
            if (handle_.promise().exception)
                std::rethrow_exception(handle_.promise().exception);
        }

      private:
        handle_type handle_;
    };
    struct promise_type {
        std::exception_ptr exception;
        std::coroutine_handle<> continuation = std::noop_coroutine();
        Task get_return_object() noexcept {
            return Task(handle_type::from_promise(*this));
        }
        std::suspend_always initial_suspend() const noexcept { return {}; }
        struct FinalAwaiter {
            bool await_ready() const noexcept { return false; }
            std::coroutine_handle<>
            await_suspend(handle_type h) const noexcept {
                return h.promise().continuation;
            }
            void await_resume() const noexcept {}
        };
        FinalAwaiter final_suspend() const noexcept { return {}; }
        void return_void() const noexcept {}
        void unhandled_exception() noexcept {
            exception = std::current_exception();
        }
    };
    Task() noexcept = default;
    explicit Task(handle_type h) noexcept : handle_(h) {}
    Task(Task &&other) noexcept : handle_(std::exchange(other.handle_, {})) {}
    Task &operator=(Task &&other) noexcept {
        if (this != &other) {
            if (handle_)
                handle_.destroy();
            handle_ = std::exchange(other.handle_, {});
        }
        return *this;
    }
    Task(const Task &) = delete;
    Task &operator=(const Task &) = delete;
    ~Task() {
        if (handle_)
            handle_.destroy();
    }
    void resume() const {
        if (handle_ && !handle_.done())
            handle_.resume();
    }
    [[nodiscard]] bool done() const noexcept {
        return handle_ && handle_.done();
    }
    [[nodiscard]] explicit operator bool() const noexcept { return !!handle_; }
    [[nodiscard]] handle_type handle() const noexcept { return handle_; }
    [[nodiscard]] Awaiter operator co_await() && {
        if (!handle_)
            throw std::logic_error("awaiting an empty Task");
        return Awaiter(std::exchange(handle_, {}));
    }

  private:
    handle_type handle_{};
};

} // namespace muduo20::coroutine
