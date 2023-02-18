#pragma once

#include <cstddef>

namespace dmt::internal {

inline constexpr bool IsPowerOfTwo(std::size_t n) {
  return n && !(n & (n - 1));
}

inline constexpr std::size_t AlignUp(std::size_t n, std::size_t alignment) {
  if (!n || !alignment)
    return 0;

  return (n + alignment - 1) & ~(alignment - 1);
}

} // namespace dmt::internal