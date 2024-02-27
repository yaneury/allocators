#pragma once

#include <cassert>
#include <cstdlib>
#include <optional>

#include "failure.hpp"
#include "util.hpp"

namespace dmt::allocator::internal {

// A successful allocation request. This POD
// contains the returned pointer and provided size
// for a request to allocate memory, e.g. through `std::malloc`.
struct Allocation {
  std::byte* base = nullptr;
  std::size_t size = 0;

  Allocation() : base(nullptr), size(0){};

  explicit Allocation(std::byte* base, std::size_t size)
      : base(base), size(size) {
    assert(base != nullptr && size != 0);
  }

  // TODO: Using Unset() and IsSet() for presence checking
  //  is odd. Consider moving to std::optional.
  void Unset() {
    base = nullptr;
    size = 0;
  }

  [[nodiscard]] constexpr bool IsSet() const {
    return base != nullptr && size != 0;
  }
};

// Gets the page size (in bytes) for the current platform.
std::size_t GetPageSize();

inline bool IsPageMultiple(std::size_t request) {
  return request >= GetPageSize() && request % GetPageSize() == 0;
}

Failable<Allocation> FetchPages(std::size_t count);

Failable<void> ReturnPages(Allocation allocation);

} // namespace dmt::allocator::internal

// TODO: Add Windows support
#if defined(__unix__) || (defined(__APPLE__) && defined(__MACH__))

#include <sys/mman.h>
#include <unistd.h>

namespace dmt::allocator::internal {

[[gnu::pure]] inline std::size_t GetPageSize() {
  return static_cast<std::size_t>(sysconf(_SC_PAGE_SIZE));
}

inline Failable<Allocation> FetchPages(std::size_t count) {
  if (count == 0)
    return cpp::fail(Failure::InvalidSize);

  std::size_t size = count * GetPageSize();

  void* ptr = mmap(NULL, size, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

  // TODO: Log platform error
  if (ptr == MAP_FAILED)
    return cpp::fail(Failure::AllocationFailed);

  return Allocation(static_cast<std::byte*>(ptr), size);
}

inline Failable<void> ReturnPages(Allocation allocation) {
  // TODO: Log platform error
  if (auto result = munmap(allocation.base, allocation.size); result != 0)
    return cpp::fail(Failure::ReleaseFailed);

  return {};
}

} // namespace dmt::allocator::internal

#endif
