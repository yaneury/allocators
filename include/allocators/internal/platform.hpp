#pragma once

#include <cstdint>
#include <cstdlib>

#include <allocators/internal/failure.hpp>
#include <allocators/internal/util.hpp>

namespace allocators::internal {

// Gets the page size (in bytes) for the current platform.
constexpr inline std::size_t GetPageSize();

// A contiguous region of page-aligned memory. The |base| pointer is guaranteed
// to be page aligned. The total size of allocated memory is determined by
// multiplying GetPageSize() with |pages|.
struct VirtualAddressRange {
  static constexpr std::size_t kMaxPageCount = (1 << 16) - 1;

  std::uint64_t address : 48;
  std::uint64_t count : 16;

  [[nodiscard]] constexpr std::size_t GetSize() const {
    return count * GetPageSize();
  }
};

Failable<VirtualAddressRange> FetchPages(std::size_t count);

Failable<void> ReturnPages(VirtualAddressRange allocation);

} // namespace allocators::internal

namespace allocators::internal {

// Apple Silicon uses 16KB page sizes. Every other supported platform uses 4KB.
#if defined(__APPLE__) && defined(__MACH__) && defined(__arm__)
// 16KB page size
constexpr inline std::size_t GetPageSize() { return 1 << 14; }
#else
// 4KB page size
constexpr inline std::size_t GetPageSize() { return 1 << 12; }
#endif

} // namespace allocators::internal

// TODO: Add Windows support
#if defined(__unix__) || (defined(__APPLE__) && defined(__MACH__))

#include <sys/mman.h>
#include <unistd.h>

namespace allocators::internal {

inline Failable<VirtualAddressRange> FetchPages(std::size_t count) {
  if (count == 0)
    return cpp::fail(Failure::InvalidSize);

  std::size_t size = count * GetPageSize();

  void* ptr = mmap(NULL, size, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

  // TODO: Log platform error
  if (ptr == MAP_FAILED)
    return cpp::fail(Failure::AllocationFailed);

  return VirtualAddressRange{.address = reinterpret_cast<std::uint64_t>(ptr),
                             .count = count};
}

inline Failable<void> ReturnPages(VirtualAddressRange allocation) {
  void* address = reinterpret_cast<void*>(allocation.address);
  // TODO: Log platform error
  if (auto result = munmap(address, allocation.GetSize()); result != 0)
    return cpp::fail(Failure::ReleaseFailed);

  return {};
}

} // namespace allocators::internal

#endif
