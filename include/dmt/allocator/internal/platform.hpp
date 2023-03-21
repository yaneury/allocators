#pragma once

#include <cstdlib>
#include <optional>

#include "util.hpp"

namespace dmt::allocator::internal {

// Gets the page size (in bytes) for the current platform.
std::size_t GetPageSize();

inline bool IsPageMultiple(std::size_t request) {
  return request >= GetPageSize() && request % GetPageSize() == 0;
}

struct Allocation {
  std::byte* base = nullptr;
  std::size_t size = 0;

  void ZeroOut() {
    this->base = nullptr;
    this->size = 0;
  }

  constexpr bool IsZeroedOut() {
    return this->base == nullptr && this->size == 0;
  }
};

inline std::optional<Allocation> AllocateBytes(std::size_t size,
                                               std::size_t alignment) {
  if (!IsValidRequest(size, alignment))
    return std::nullopt;

  void* ptr = std::aligned_alloc(alignment, size);
  if (!ptr)
    return std::nullopt;

  return Allocation({.base = static_cast<std::byte*>(ptr), .size = size});
}

inline void ReleaseBytes(Allocation allocation) {
  if (!allocation.base)
    return;

  void* ptr = static_cast<void*>(allocation.base);
  std::free(ptr);
}

// TODO: Add error handling.
std::optional<Allocation> AllocatePages(std::size_t pages);
void ReleasePages(Allocation allocation);

} // namespace dmt::allocator::internal

#if defined(__unix__) || (defined(__APPLE__) && defined(__MACH__))

#include <sys/mman.h>
#include <unistd.h>

namespace dmt::allocator::internal {

inline std::size_t GetPageSize() {
  return static_cast<std::size_t>(sysconf(_SC_PAGE_SIZE));
}

inline std::optional<Allocation> AllocatePages(std::size_t pages) {
  if (pages == 0)
    return std::nullopt;

  std::size_t size = pages * GetPageSize();
  void* ptr = mmap(NULL, size, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (ptr == MAP_FAILED)
    return std::nullopt;

  return Allocation({.base = static_cast<std::byte*>(ptr), .size = size});
}

inline void ReleasePages(Allocation allocation) {
  munmap(allocation.base, allocation.size);
}

} // namespace dmt::allocator::internal

#endif
