// Named template parameters used by the memory allocators in this library.

#pragma once

#include <cstddef>
#include <libdmt/internal/types.hpp>

namespace dmt::allocator {

// A single byte.
using Byte = uint8_t;

// Default size (in bytes) for a single chunk allocated by the Bump allocator.
static constexpr std::size_t kDefaultSize = 4096;

struct SizeId {};

// Size (in bytes) for allocator's chunks. Usually, an allocator uses fixed-size
// chunks to allocate memory. Within a chunk, several objects of varying length
// can be "allocated", so long as they fit within the amount of memory already
// acquired. Currently, this field is used by the Bump allocator.
template <std::size_t Size>
struct SizeT : std::integral_constant<std::size_t, Size> {
  using Id_ = SizeId;
};

struct AlignmentId {};

// Alignment used when making an allocation. Usually, allocators defer to the
// alignment of the underlying object they are allocating. The constrains for
// this value are that it is a power of two and greater than |sizeof(void*)|.
template <std::size_t Alignment>
struct AlignmentT : std::integral_constant<std::size_t, Alignment> {
  using Id_ = AlignmentId;
};

struct WhenFullId {};

// Policy to employ when Bump allocator's chunk is out of space and
// can't fullfill an allocation request.
enum WhenFull {
  // Return |nullptr|, signalling that no more allocations can be made on the
  // allocator until a free request
  // is made.
  ReturnNull = 0,

  // Grow storage by requesting a new chunk to allocate the request and
  // subsequent requests in.
  GrowStorage = 1
};

template <WhenFull WF> struct GrowT : std::integral_constant<WhenFull, WF> {
  using Id_ = WhenFullId;
};

} // namespace dmt::allocator