#pragma once

#ifdef DMT_DEBUG

#include <plog/Log.h>

#define DINFO(x) PLOGD << x;
#define DERROR(x) PLOGE << x;
#else
#define DINFO(x)
#define DERROR(x)
#endif

#include <cstddef>

namespace dmt::allocator::internal {

static constexpr size_t kMinimumAlignment = sizeof(void*);

[[gnu::const]] inline constexpr bool IsPowerOfTwo(std::size_t n) {
  return n && !(n & (n - 1));
}

[[gnu::const]] inline constexpr std::size_t AlignUp(std::size_t n,
                                                    std::size_t alignment) {
  if (!n || !alignment)
    return 0;

  return (n + alignment - 1) & ~(alignment - 1);
}

[[gnu::const]] inline constexpr std::size_t AlignDown(std::size_t n,
                                                      std::size_t alignment) {
  if (!n || !alignment)
    return 0;

  return (n & ~(alignment - 1));
}

[[gnu::const]] inline constexpr bool IsValidAlignment(std::size_t alignment) {
  return alignment >= kMinimumAlignment && IsPowerOfTwo(alignment);
}

[[gnu::const]] inline bool IsValidRequest(std::size_t size,
                                          std::size_t alignment) {
  return size && IsValidAlignment(alignment);
}

} // namespace dmt::allocator::internal
