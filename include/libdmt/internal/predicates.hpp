#pragma once

#include <cstddef>

namespace dmt {
namespace internal {

inline constexpr bool IsPowerOfTwo(std::size_t n) {
  return n && !(n & (n - 1));
}

} // namespace internal
} // namespace dmt