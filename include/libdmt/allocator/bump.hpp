#pragma once

#include <array>
#include <cstdlib>

namespace dmt {
namespace allocator {

static constexpr std::size_t kDefaultStorageSize = 4096;

// TODO: Lazy v. Eager Allocation
// TODO: Static Buffer vs Heap
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
    if (n > StorageSize)
      return nullptr;

    if (!storage_) {
      if (storage_ = static_cast<Byte *>(std::malloc(StorageSize)); !storage_) {
        return nullptr;
      }
    }

    size_t request_size = AlignUp(n);
    size_t remaining_size = StorageSize - offset;

    if (request_size > remaining_size)
      return nullptr;

    Byte *result = storage_ + offset;
    offset += request_size;

    return reinterpret_cast<T *>(result);
  }

  void deallocate(T *p, std::size_t n) { /* no op */
  }

private:
  using Byte = uint8_t;

  std::size_t AlignUp(std::size_t n, std::size_t alignment = 2) {
    if (n < alignment)
      return alignment;

    std::size_t remainder = n % alignment;
    return remainder ? n + (alignment - remainder) : n;
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