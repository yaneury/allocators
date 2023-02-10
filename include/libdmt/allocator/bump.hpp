#pragma once

#include <array>
#include <cstdlib>
#include <libdmt/internal/predicates.hpp>
#include <libdmt/internal/types.hpp>

namespace dmt {
namespace allocator {

static constexpr std::size_t kDefaultStorageSize = 4096;

struct StorageSizeId_ {};

template <std::size_t Size>
struct StorageSizeT : std::integral_constant<std::size_t, Size> {
  using Id_ = StorageSizeId_;
};

struct AlignmentId_ {};

template <std::size_t Alignment>
struct AlignmentT : std::integral_constant<std::size_t, Alignment> {
  using Id_ = AlignmentId_;
};

// TODO: Arena allocations when at capacity and using heap
// TODO: Implement free
template <class T, typename... Args> class Bump {
public:
  // Require alias for std::allocator_traits to infer other types, e.g.
  // using pointer = value_type*.
  using value_type = T;

  explicit Bump(){};

  ~Bump() {
    if (head_)
      std::free(head_);
  }

  template <class U> constexpr Bump(const Bump<U>&) noexcept {}

  T* allocate(std::size_t n) {
    if (n > AlignedStorageSize_)
      return nullptr;

    if (!head_) {
      if (head_ = static_cast<Byte*>(
              std::aligned_alloc(Alignment_, AlignedStorageSize_));
          !head_) {
        return nullptr;
      }
    }

    size_t request_size = AlignUp(n, Alignment_);
    size_t remaining_size = AlignedStorageSize_ - offset;

    if (request_size > remaining_size)
      return nullptr;

    Byte* result = head_ + offset;
    offset += request_size;

    return reinterpret_cast<T*>(result);
  }

  void deallocate(T* p, std::size_t n) { /* no op */
  }

private:
  using Byte = uint8_t;

  // There are several factors used to determine the alignment for the
  // allocator. First, users can specify their own alignment if desired using
  // |AlignmentT<>|. Otherwise, we use the alignment as determined by the C++
  // compiler. There's a floor in the size of the alignment to be equal to or
  // greater than |sizeof(void*)| for compatibility with std::aligned_alloc.
  static constexpr std::size_t Alignment_ =
      std::max({std::alignment_of_v<T>, sizeof(void*),
                dmt::internal::GetValueT<AlignmentT<0>, Args...>::value});

  static_assert(dmt::internal::IsPowerOfTwo(Alignment_),
                "Alignment must be a power of 2.");

  static constexpr std::size_t AlignedStorageSize_ =
      ((dmt::internal::GetValueT<StorageSizeT<kDefaultStorageSize>,
                                 Args...>::value -
        1) |
       (Alignment_ - 1)) +
      1;

  std::size_t AlignUp(std::size_t n, std::size_t alignment) {
    return (n + alignment - 1) & ~(alignment - 1);
  }

  size_t offset = 0;
  Byte* head_ = nullptr;
};

template <class T, class U> bool operator==(const Bump<T>&, const Bump<U>&) {
  return true;
}

template <class T, class U> bool operator!=(const Bump<T>&, const Bump<U>&) {
  return false;
}

} // namespace allocator

} // namespace dmt