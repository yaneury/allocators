// Named template parameters used by the memory allocators in this library.

#pragma once

#include <cstddef>
#include <type_traits>

namespace dmt::allocator {

// Size (in bytes) for allocator's chunks. Usually, an allocator uses fixed-size
// chunks to allocate memory. Within a chunk, several objects of varying length
// can be "allocated", so long as they fit within the amount of memory already
// acquired. Currently, this field is used by the Bump allocator.
template <std::size_t Size>
struct SizeT : std::integral_constant<std::size_t, Size> {};

// Alignment used when making an allocation. Usually, allocators defer to the
// alignment of the underlying object they are allocating. The constrains for
// this value are that it is a power of two and greater than |sizeof(void*)|.
template <std::size_t Alignment>
struct AlignmentT : std::integral_constant<std::size_t, Alignment> {};

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

template <WhenFull WF> struct GrowT : std::integral_constant<WhenFull, WF> {};

// Policy used to determine sizing constraint for chunks.
enum ChunksMust {
  // Chunk must have enough space to fulfill requested size in |SizeT|
  // parameter.
  HaveAtLeastSizeBytes = 0,

  // Chunk can not be larger than requested size in |SizeT| parameter.
  NoMoreThanSizeBytes = 1,
};

template <ChunksMust CM>
struct LimitT : std::integral_constant<ChunksMust, CM> {};

// Policy to employ when looking for free chunk in free list.
enum FindBy {
  // Use first chunk that contains the minimum sizes of bytes.
  FirstFit,

  // Use the *smallest* chunk that contains the minimum sizes of bytes.
  BestFit,
  //
  // Use the *largest* chunk that contains the minimum sizes of bytes.
  WorstFit
};

template <FindBy FB> struct SearchT : std::integral_constant<FindBy, FB> {};

} // namespace dmt::allocator
