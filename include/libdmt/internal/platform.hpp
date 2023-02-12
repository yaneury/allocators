#pragma once

#include <cstddef>

namespace dmt::internal {

// Gets the page size (in bytes) for the current platform.
std::size_t GetPageSize();

} // namespace dmt::internal

#if __APPLE__

#include <cassert>
#include <cstdlib>
#include <optional>
#include <sys/mman.h>
#include <unistd.h>

namespace dmt::internal {

std::size_t GetPageSize() { return static_cast<std::size_t>(getpagesize()); }

using Byte = uint8_t;

class Allocation {
public:
  explicit Allocation(Byte* base, std::size_t size) : base_(base), size_(size) {
    assert(base_ != nullptr);
    assert(size_ > 0);
  }

  Byte* GetPtr() const { return base_; }

  std::size_t GetSize() const { return size_; }

private:
  Byte* base_;
  std::size_t size_;
};

class ObjectAllocator {
public:
  static std::optional<Allocation> Allocate(std::size_t size,
                                            std::size_t alignment) {
    // TODO: Add more validation: alignment < size, alignment is power of two,
    // etc.
    if (size == 0 || alignment == 0)
      return std::nullopt;

    void* ptr = std::aligned_alloc(alignment, size);
    if (!ptr)
      return std::nullopt;

    return Allocation(static_cast<Byte*>(ptr), size);
  }

  static void Release(Allocation allocation) {
    // TODO: Add validation
    std::free(allocation.GetPtr());
  }
};

class PageAllocator {
public:
  static std::optional<Allocation> Allocate(std::size_t pages) {
    if (pages == 0)
      return std::nullopt;

    std::size_t size = pages * GetPageSize();
    void* ptr = mmap(NULL, size, PROT_READ | PROT_WRITE,
                     MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (ptr == MAP_FAILED)
      return std::nullopt;

    return Allocation(static_cast<Byte*>(ptr), size);
  }

  static void Release(Allocation allocation) {
    munmap(allocation.GetPtr(), allocation.GetSize());
  }

private:
};

} // namespace dmt::internal

#endif