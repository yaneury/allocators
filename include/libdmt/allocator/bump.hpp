#pragma once

#include <array>
#include <cstdlib>
#include <libdmt/internal/platform.hpp>
#include <libdmt/internal/types.hpp>
#include <libdmt/internal/util.hpp>

namespace dmt::allocator {

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

enum Free { Never = 0, WhenCounterZero = 1 };

struct FreeId_ {};

template <Free F> struct FreeT : std::integral_constant<Free, F> {
  using Id_ = FreeId_;
};

// TODO: Arena allocations when at capacity and using heap
template <class T, typename... Args> class Bump {
public:
  // Require alias for std::allocator_traits to infer other types, e.g.
  // using pointer = value_type*.
  using value_type = T;

  explicit Bump(){};

  ~Bump() { Reset(); }

  template <class U> constexpr Bump(const Bump<U>&) noexcept {}

  T* allocate(std::size_t n) {
    if (n > AlignedStorageSize_)
      return nullptr;

    size_t request_size = dmt::internal::AlignUp(n, Alignment_);
    size_t remaining_size = AlignedStorageSize_ - offset_;

    if (request_size > remaining_size)
      return nullptr;

    if (!chunk_.has_value()) {
      chunk_ = internal::AllocateObjects(AlignedStorageSize_, Alignment_);
      if (!chunk_.has_value())
        return nullptr;
    }

    Byte* result = chunk_->base + offset_;
    offset_ += request_size;

    if constexpr (FreeStrategy_ == Free::WhenCounterZero)
      allocation_counter_ += 1;

    return reinterpret_cast<T*>(result);
  }

  void deallocate(T*, std::size_t) {
    if constexpr (FreeStrategy_ == Free::WhenCounterZero) {
      if (!allocation_counter_ || !chunk_.has_value())
        return;

      allocation_counter_ -= 1;
      if (allocation_counter_)
        return;

      offset_ = 0;
      internal::ReleaseObjects(chunk_.value());
      chunk_ = std::nullopt;
    }
  }

  void Reset() {
    offset_ = 0;
    if constexpr (FreeStrategy_ == Free::WhenCounterZero)
      allocation_counter_ = 0;
    if (chunk_.has_value())
      internal::ReleaseObjects(chunk_.value());
    chunk_ = std::nullopt;
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

  static constexpr Free FreeStrategy_ =
      dmt::internal::GetValueT<FreeT<Free::Never>, Args...>::value;

  size_t offset_ = 0;
  std::optional<internal::Allocation> chunk_ = std::nullopt;

  struct Empty {};
  using Counter = std::conditional_t<FreeStrategy_ == Free::WhenCounterZero,
                                     std::size_t, Empty>;
  [[no_unique_address]] Counter allocation_counter_ = Counter();
};

template <class T, class U> bool operator==(const Bump<T>&, const Bump<U>&) {
  return true;
}

template <class T, class U> bool operator!=(const Bump<T>&, const Bump<U>&) {
  return false;
}

} // namespace dmt::allocator