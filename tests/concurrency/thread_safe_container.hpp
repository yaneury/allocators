#pragma once

#include <mutex>
#include <optional>
#include <queue>

namespace dmt::testing {

template <class T> class ThreadSafeQueue {
public:
  void Push(T&& t) {
    std::lock_guard<std::mutex> guard(lock_);
    queue_.push(std::move(t));
  }

  void Push(const T& t) {
    std::lock_guard<std::mutex> guard(lock_);
    queue_.push(t);
  }

  std::optional<T> Pop() {
    std::lock_guard<std::mutex> guard(lock_);

    if (queue_.empty())
      return std::nullopt;

    T value = queue_.front();
    queue_.pop();
    return value;
  }

private:
  std::mutex lock_;
  std::queue<T> queue_;
};
} // namespace dmt::testing