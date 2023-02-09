#pragma once

#include <array>
#include <cstdlib>

namespace dmt {
namespace allocator {

static constexpr std::size_t kDefaultStorageSize = 4096;

// TODO: Lazy v. Eager Allocation
// TODO: Static Buffer vs Heap
// TODO: Arena allocations when at capacity and using heap
template <class T, std::size_t StorageSize = kDefaultStorageSize> class Bump {
public:
  // Require alias for std::allocator_traits to infer other types, e.g.
  // using pointer = value_type*.
  using value_type = T;

  explicit Bump(){};

  ~Bump() {
    if (storage_)
      std::free(storage_);
  }

  template <class U> constexpr Bump(const Bump<U> &) noexcept {}

  T *allocate(std::size_t n) {
    if (n > AlignedStorageSize_)
      return nullptr;

    if (!storage_) {
      if (storage_ = static_cast<Byte_ *>(
              std::aligned_alloc(Alignment_, AlignedStorageSize_));
          !storage_) {
        return nullptr;
      }
    }

    size_t request_size = AlignUp(n, Alignment_);
    size_t remaining_size = AlignedStorageSize_ - offset;

    if (request_size > remaining_size)
      return nullptr;

    Byte *result = storage_ + offset;
    offset += request_size;

    return reinterpret_cast<T *>(result);
  }

  void deallocate(T *p, std::size_t n) { /* no op */
  }

private:
  using Byte_ = uint8_t;
  static constexpr std::size_t Alignment_ =
      std::max(std::alignment_of_v<T>, sizeof(void *));
  static constexpr std::size_t AlignedStorageSize_ =
      ((StorageSize - 1) | (Alignment - 1)) + 1;

  std::size_t AlignUp(std::size_t n, std::size_t alignment) {
    return (n + alignment - 1) & ~(alignment - 1);
  }

  size_t offset = 0;
  Byte *storage_ = nullptr;
};

template <class T, class U> bool operator==(const Bump<T> &, const Bump<U> &) {
  return true;
}

template <class T, class U> bool operator!=(const Bump<T> &, const Bump<U> &) {
  return false;
}

} // namespace allocator

} // namespace dmt