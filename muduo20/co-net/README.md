# co-net

An isolated copy of the muduo20 networking layer for coroutine-oriented evolution. The original `muduo20/net` implementation remains unchanged.

- C++ namespace: `muduo20::co_net`
- CMake target: `muduo20_co_net` (alias `muduo20::co_net`)
- Include prefix: `muduo20/co-net/`
- Coroutine primitives: `muduo20/co-net/coroutine/`

The copied network implementation shares `muduo20_base` and existing third-party dependencies.
