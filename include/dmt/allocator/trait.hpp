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

// Returning std::byte* is odd because it implies the return value
// is a pointer to a std::byte which is not true ever. Instead, it's
// an address (8 bytes on x86_64).
// TODO: Return void* or something that is more explicitly a word size addr.
template <class T>
concept Trait = requires(T allocator, std::size_t size, Layout layout,
                         std::byte* bytes) {
  { allocator.Allocate(layout) } -> std::same_as<Result<std::byte*>>;
  { allocator.Allocate(size) } -> std::same_as<Result<std::byte*>>;
  { allocator.Release(bytes) } -> std::same_as<Result<void>>;
};

#endif

} // namespace dmt::allocator
