#pragma once

#include <concepts>
#include <muduo20/base/noncopyable.h>

namespace muduo20 {

template <typename T>
  requires std::default_initializable<T>
class Singleton : noncopyable {
public:
  static T &instance() {
    static T value{};
    return value;
  }
};

} // namespace muduo20
