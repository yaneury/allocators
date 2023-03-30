// Named template parameters used by the memory allocators in this library.

#pragma once

#include <cstddef>
#include <type_traits>

#include <template/parameters.hpp>

#include "trait.hpp"

namespace dmt::allocator {

// Size (in bytes) for allocator's blocks. Usually, an allocator uses fixed-size
// blocks to allocate memory. Within a block, several objects of varying length
// can be "allocated", so long as they fit within the amount of memory already
// acquired. Currently, this field is used by the Bump allocator.
template <std::size_t Size>
struct SizeT : std::integral_constant<std::size_t, Size> {};

// Alignment used when making an allocation. Usually, allocators defer to the
// alignment of the underlying object they are allocating. The constrains for
// this value are that it is a power of two and greater than |sizeof(void*)|.
template <std::size_t Alignment>
struct AlignmentT : std::integral_constant<std::size_t, Alignment> {};

// Policy to employ when Bump allocator's block is out of space and
// can't fullfill an allocation request.
enum WhenFull {
  // Return |nullptr|, signalling that no more allocations can be made on the
  // allocator until a free request
  // is made.
  ReturnNull = 0,

  // Grow storage by requesting a new block to allocate the request and
  // subsequent requests in.
  GrowStorage = 1
};

template <WhenFull WF> struct GrowT : std::integral_constant<WhenFull, WF> {};

// Policy used to determine sizing constraint for blocks.
enum BlocksMust {
  // Block must have enough space to fulfill requested size in |SizeT|
  // parameter.
  HaveAtLeastSizeBytes = 0,

  // Block can not be larger than requested size in |SizeT| parameter.
  NoMoreThanSizeBytes = 1,
};

template <BlocksMust CM>
struct LimitT : std::integral_constant<BlocksMust, CM> {};

// Policy to employ when looking for free block in free list.
enum FindBy {
  // Use first block that contains the minimum sizes of bytes.
  FirstFit = 0,

  // Use the *smallest* block that contains the minimum sizes of bytes.
  BestFit = 1,
  //
  // Use the *largest* block that contains the minimum sizes of bytes.
  WorstFit = 2
};

template <FindBy FB> struct SearchT : std::integral_constant<FindBy, FB> {};

// Max number of requests that an individual PageAllocator will keep track of.
// This size is used for allocating memory for internal bookkeeping.
template <std::size_t R>
struct RequestT : std::integral_constant<std::size_t, R> {};

// Allocator type used to fetch blocks of memory.
template <Trait Allocator> struct AllocatorT : ntp::integral_type<Allocator> {};

} // namespace dmt::allocator

// Macro-based defaults

#ifndef DMT_ALLOCATOR_ALIGNMENT
#define DMT_ALLOCATOR_ALIGNMENT sizeof(void*)
#endif

#ifndef DMT_ALLOCATOR_SIZE
#define DMT_ALLOCATOR_SIZE 4096ul
#endif

#ifndef DMT_ALLOCATOR_BLOCKS_MUST
#define DMT_ALLOCATOR_BLOCKS_MUST 0
#endif

#if DMT_ALLOCATOR_BLOCKS_MUST == 0
#define DMT_ALLOCATOR_LIMIT BlocksMust::HaveAtLeastSizeBytes
#elif DMT_ALLOCATOR_BLOCKS_MUST == 1
#define DMT_ALLOCATOR_LIMIT BlocksMust::NoMoreThanSizeBytes
#else
#error "Only values 0 or 1 can be provided"
#endif

#ifndef DMT_ALLOCATOR_WHEN_FULL
#define DMT_ALLOCATOR_WHEN_FULL 0
#endif

#if DMT_ALLOCATOR_WHEN_FULL == 0
#define DMT_ALLOCATOR_GROW WhenFull::GrowStorage
#elif DMT_ALLOCATOR_WHEN_FULL == 1
#define DMT_ALLOCATOR_GROW WhenFull::ReturnNull
#else
#error "Only values 0 or 1 can be provided"
#endif
