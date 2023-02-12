#pragma once

#include <cstdlib>
#include <optional>

namespace dmt::internal {

using Byte = uint8_t;

// Gets the page size (in bytes) for the current platform.
std::size_t GetPageSize();

struct Allocation {
  Byte* base;
  std::size_t size;
};

std::optional<Allocation> AllocateBytes(std::size_t size,
                                        std::size_t alignment) {
  // TODO: Add more validation: alignment < size, alignment is power of two,
  // etc.
  if (size == 0 || alignment == 0)
    return std::nullopt;

  void* ptr = std::aligned_alloc(alignment, size);
  if (!ptr)
    return std::nullopt;

  return Allocation({.base = static_cast<Byte*>(ptr), .size = size});
}

void ReleaseBytes(Allocation allocation) {
  // TODO: Add validation
  std::free(allocation.base);
}

std::optional<Allocation> AllocatePages(std::size_t pages);
void ReleasePages(Allocation allocation);

} // namespace dmt::internal

#if defined(__unix__) || (defined(__APPLE__) && defined(__MACH__))

#include <sys/mman.h>
#include <unistd.h>

namespace dmt::internal {

std::size_t GetPageSize() {
  return static_cast<std::size_t>(sysconf(_SC_PAGE_SIZE));
}

std::optional<Allocation> AllocatePages(std::size_t pages) {
  if (pages == 0)
    return std::nullopt;

  std::size_t size = pages * GetPageSize();
  void* ptr = mmap(NULL, size, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (ptr == MAP_FAILED)
    return std::nullopt;

  return Allocation({.base = static_cast<Byte*>(ptr), .size = size});
}

void ReleasePages(Allocation allocation) {
  munmap(allocation.base, allocation.size);
}

} // namespace dmt::internal

#endif