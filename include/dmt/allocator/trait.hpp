#pragma once

#include <cstddef>

#include "error.hpp"
#include "internal/util.hpp"

#if __cplusplus >= 202002L
#include <concepts>
#endif

namespace dmt::allocator {

// A parameter used for making an allocation request.
struct Layout {
  // Number of bytes requested for allocation.
  std::size_t size;

  // Alignment for allocated bytes.
  // Value must be a power of two and greater than or equal to
  // the current running architecture's word size, i.e. `sizeof(void*)`.
  std::size_t alignment;

  explicit Layout(std::size_t size, std::size_t alignment)
      : size(size), alignment(alignment) {}
};

[[gnu::const]] inline bool IsValid(Layout layout) {
  return internal::IsValidRequest(layout.size, layout.alignment);
}

#if __cplusplus >= 202002L

template <class T>
concept ObjectAllocator =
    requires(T allocator, std::size_t size, Layout layout, std::byte* bytes) {
      { allocator.Allocate(layout) } -> std::same_as<Result<std::byte*>>;
      { allocator.Allocate(size) } -> std::same_as<Result<std::byte*>>;
      { allocator.Release(bytes) } -> std::same_as<Result<void>>;
    };

template <class T>
concept BlockAllocator =
    requires(T allocator, std::size_t count, std::byte* bytes) {
      { allocator.Allocate(count) } -> std::same_as<Result<std::byte*>>;
      { allocator.Release(bytes) } -> std::same_as<Result<void>>;
    };

#endif

} // namespace dmt::allocator
