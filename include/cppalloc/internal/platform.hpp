#pragma once

#include <cassert>
#include <cstdlib>
#include <optional>

#include "failure.hpp"
#include "util.hpp"

namespace cppalloc::internal {

// Gets the page size (in bytes) for the current platform.
constexpr inline std::size_t GetPageSize();

// A contiguous region of page-aligned memory. The |base| pointer is guaranteed
// to be page aligned. The total size of allocated memory is determined by
// multiplying GetPageSize() with |pages|.
struct VirtualAddressRange {
  std::byte* base = nullptr;
  std::size_t pages = 0;

  VirtualAddressRange() : base(nullptr), pages(0){};

  constexpr explicit VirtualAddressRange(std::byte* base, std::size_t pages)
      : base(base), pages(pages) {
    // This should never happen.
    assert(base != nullptr && pages != 0);
  }

  [[nodiscard]] constexpr std::size_t GetSize() const {
    return pages * GetPageSize();
  }
};

Failable<VirtualAddressRange> FetchPages(std::size_t count);

Failable<void> ReturnPages(VirtualAddressRange allocation);

} // namespace cppalloc::internal

namespace cppalloc::internal {

// Apple Silicon uses 16KB page sizes. Every other supported platform uses 4KB.
#if defined(__APPLE__) && defined(__MACH__) && defined(__arm__)
// 16KB page size
constexpr inline std::size_t GetPageSize() { return 1 << 14; }
#else
// 4KB page size
constexpr inline std::size_t GetPageSize() { return 1 << 12; }
#endif

} // namespace cppalloc::internal

// TODO: Add Windows support
#if defined(__unix__) || (defined(__APPLE__) && defined(__MACH__))

#include <sys/mman.h>
#include <unistd.h>

namespace cppalloc::internal {

inline Failable<VirtualAddressRange> FetchPages(std::size_t count) {
  if (count == 0)
    return cpp::fail(Failure::InvalidSize);

  std::size_t size = count * GetPageSize();

  void* ptr = mmap(NULL, size, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

  // TODO: Log platform error
  if (ptr == MAP_FAILED)
    return cpp::fail(Failure::AllocationFailed);

  return VirtualAddressRange(static_cast<std::byte*>(ptr), size);
}

inline Failable<void> ReturnPages(VirtualAddressRange allocation) {
  // TODO: Log platform error
  if (auto result = munmap(allocation.base, allocation.GetSize()); result != 0)
    return cpp::fail(Failure::ReleaseFailed);

  return {};
}

} // namespace cppalloc::internal

#endif
