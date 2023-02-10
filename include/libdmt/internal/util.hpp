#pragma once

#include <cstddef>

namespace dmt {
namespace internal {

inline constexpr bool IsPowerOfTwo(std::size_t n) {
  return n && !(n & (n - 1));
}

inline std::size_t AlignUp(std::size_t n, std::size_t alignment) {
  return (n + alignment - 1) & ~(alignment - 1);
}

} // namespace internal
} // namespace dmt