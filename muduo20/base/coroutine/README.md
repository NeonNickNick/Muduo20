# muduo20 coroutine primitives

This directory contains header-only C++20 coroutine primitives. The core uses only the C++ standard library and is independent of muduo20 networking.

## Task

`Task<T>` and `Task<void>` are lazy, move-only coroutine return types. A task
starts when it is resumed or awaited. Awaiting a child task transfers execution
to that child; when it completes, execution transfers back to the awaiting
coroutine. Values and exceptions are delivered to the awaiting coroutine.

```cpp
#include "muduo20/base/coroutine/Task.h"

muduo20::coroutine::Task<int> value() {
  co_return 42;
}

muduo20::coroutine::Task<void> use_value() {
  const int result = co_await value();
  (void)result;
  co_return;
}
```

A top-level task can be started with `resume()` and inspected with `done()`.
An external executor may retain the task and call `resume()` when an awaited
operation signals readiness. It must resume a suspended task only when that
operation is ready. `Executor` can schedule the initial resume of a caller-owned task.

## Ownership and errors

A `Task` owns its coroutine frame. It is not copyable; moving transfers that
ownership. Destroying a task destroys its frame. Awaiting consumes the task and
the parent owns the child frame until the await completes. Awaiting an empty
task throws `std::logic_error`. Exceptions thrown by a task are rethrown in its
awaiter.

`OperationAwaiter` bridges a callback-based async operation. It stores a cancel function and accepts `std::stop_token`; destroying its coroutine frame invokes cancellation and prevents queued completion from resuming a dead frame. The co-net adapter lives separately under `muduo20/co-net/coroutine`.

## Requirements

- C++20 compiler with `<coroutine>` support
- No third-party dependencies

## Executor and callback operations

`Executor` accepts a scheduler callback and has no network dependency. `start(Task<void>&)` queues initial execution while the caller retains task ownership until completion. `OperationAwaiter` adapts callback-based asynchronous operations and accepts a cancellation function plus `std::stop_token`. Completion resumes through the executor. Schedulers must defer work and run it on the coroutine owning thread.
