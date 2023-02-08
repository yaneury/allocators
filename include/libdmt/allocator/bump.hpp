#pragma once

#include <cstdlib>

namespace dmt {
namespace allocator {
template <class T> class Bump {
public:
  // Require alias for std::allocator_traits to infer other types, e.g.
  // using pointer = value_type*.
  using value_type = T;

  explicit Bump(){};

  template <class U> constexpr Bump(const Bump<U> &) noexcept {}

  T *allocate(std::size_t n) { return static_cast<T *>(std::malloc(n)); }

  void deallocate(T *p, std::size_t n) { std::free(p); }
};

template <class T, class U> bool operator==(const Bump<T> &, const Bump<U> &) {
  return true;
}

template <class T, class U> bool operator!=(const Bump<T> &, const Bump<U> &) {
  return false;
}

} // namespace allocator

} // namespace dmt