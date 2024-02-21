// The BlockHeader class and associated functions.
// A block is block of memory allocated by a memory allocator.
// The bulk of the bytes in the block are reserved for direct
// usage by the user requesting the memory. A small portion is
// reserved in the beginning of the block to contain necessary
// metadata for tracking blocks. This metadata is encapsulated
// in the BlockHeader class.

#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <optional>
#include <strings.h>

#include <result.hpp>

#include "failure.hpp"
#include "platform.hpp"

namespace dmt::allocator::internal {

// A BlockHeader contains the necessary metadata used to track
// a block of memory. The block is the bytes starting from
// the address of |BlockHeader*| up to and including the
// size of the block, stored in the |size| field.
struct BlockHeader {
  // The size of the entire block. The size includes the portion
  // of the block used up by this header.
  std::size_t size = 0;

  // The next block in the list of blocks. Blocks are kept in a
  // standard singly-linked list.
  BlockHeader* next = nullptr;

  // Cast |allocation| to |BlockHeader*|.
  static inline BlockHeader* Create(Allocation allocation,
                                    BlockHeader* next = nullptr) {
    assert(allocation.base != nullptr && allocation.size != 0);

    BlockHeader* header = reinterpret_cast<BlockHeader*>(allocation.base);
    header->size = allocation.size;
    header->next = next;
    return header;
  }
};

// A pair of BlockHeader* where the |prev| is guaranteed to have its |next|
// field set to |header|.
struct HeaderPair {
  BlockHeader* prev = nullptr;
  BlockHeader* header = nullptr;

  explicit HeaderPair(BlockHeader* header, BlockHeader* prev = nullptr)
      : header(header), prev(prev) {
    assert(header != nullptr);
  }
};

// Convenience cast functions
template <class T> inline std::byte* AsBytePtr(T* ptr) {
  return reinterpret_cast<std::byte*>(ptr);
}

template <class T> [[gnu::const]] inline constexpr std::uintptr_t AsUint(T* p) {
  return reinterpret_cast<std::uintptr_t>(p);
}

// Fixed size of Block header.
inline constexpr std::size_t GetBlockHeaderSize() {
  return sizeof(BlockHeader);
}

// Size of block when not accounting for header.
inline std::size_t BlockSize(BlockHeader* header) {
  if (!header)
    return 0;

  return header->size - GetBlockHeaderSize();
}

// Get pointer to block referenced by |header|.
inline std::byte* GetBlock(BlockHeader* header) {
  assert(header != nullptr);

  return AsBytePtr(header) + GetBlockHeaderSize();
}

// Get header from block referenced by |ptr|.
inline BlockHeader* GetHeader(std::byte* ptr) {
  assert(ptr != nullptr);

  return reinterpret_cast<BlockHeader*>(ptr - GetBlockHeaderSize());
}

// Zero out the contents of the block referenced by |header|.
inline void ZeroBlock(BlockHeader* header) {
  if (!header)
    return;

  std::byte* base = AsBytePtr(header) + GetBlockHeaderSize();
  std::size_t size = header->size - GetBlockHeaderSize();
  bzero(base, size);
}

// Cast |BlockHeader*| pointed by head to |Allocation| objects and free their
// associated memory using |release|.
inline Failable<void>
ReleaseBlockList(BlockHeader* head,
                 std::function<Failable<void>(std::byte*)> release,
                 BlockHeader* sentinel = nullptr) {
  if (head == nullptr)
    return cpp::fail(Failure::HeaderIsNullptr);

  BlockHeader* itr = head;
  while (itr != sentinel) {
    BlockHeader* next = itr->next;
    if (auto result = release(AsBytePtr(itr)); result.has_error())
      return cpp::fail(result.error());

    itr = next;
  }

  return {};
}

// Return first header that has at least |minimum_size| bytes available.
inline Failable<std::optional<HeaderPair>>
FindBlockByFirstFit(BlockHeader* head, std::size_t minimum_size) {
  if (head == nullptr)
    return cpp::fail(Failure::HeaderIsNullptr);

  if (minimum_size == 0)
    return cpp::fail(Failure::InvalidSize);

  for (BlockHeader *itr = head, *prev = nullptr; itr != nullptr;
       prev = itr, itr = itr->next)
    if (itr->size >= minimum_size)
      return HeaderPair(itr, prev);

  return std::nullopt;
}

// Return header that most closely fits |minimum_size| using |cmp| as the
// criteria.
inline Failable<std::optional<HeaderPair>>
FindBlockByFit(BlockHeader* head, std::size_t minimum_size,
               std::function<bool(std::size_t, std::size_t)> cmp) {
  if (head == nullptr)
    return cpp::fail(Failure::HeaderIsNullptr);

  if (minimum_size == 0)
    return cpp::fail(Failure::InvalidSize);

  std::optional<HeaderPair> target = std::nullopt;
  for (BlockHeader *itr = head, *prev = nullptr; itr != nullptr;
       prev = itr, itr = itr->next) {
    if (itr->size < minimum_size)
      continue;

    if (!target.has_value() || cmp(itr->size, target->header->size)) {
      target = HeaderPair(itr, prev);
    }
  }
  return target;
}

inline Failable<std::optional<HeaderPair>>
FindBlockByBestFit(BlockHeader* head, std::size_t minimum_size) {
  return FindBlockByFit(
      head, minimum_size,
      /*cmp=*/[](std::size_t a, std::size_t b) { return a < b; });
}

inline Failable<std::optional<HeaderPair>>
FindBlockByWorstFit(BlockHeader* head, std::size_t minimum_size) {
  return FindBlockByFit(
      head, minimum_size,
      /*cmp=*/[](std::size_t a, std::size_t b) { return a > b; });
}

inline Failable<BlockHeader*> FindPriorBlock(BlockHeader* head,
                                             BlockHeader* block) {
  if (block == nullptr || head == nullptr)
    return cpp::fail(Failure::HeaderIsNullptr);

  if (AsUint(head) >= AsUint(block))
    return nullptr;

  BlockHeader* itr = head;
  while (itr->next && AsUint(itr->next) < AsUint(block))
    itr = itr->next;

  return itr;
}

// Split block and return new |BlockHeader*|.
inline Failable<BlockHeader*> SplitBlock(BlockHeader* block,
                                         std::size_t bytes_needed,
                                         std::size_t alignment) {
  if (!block)
    return cpp::fail(Failure::HeaderIsNullptr);
  if (!bytes_needed)
    return cpp::fail(Failure::InvalidSize);
  if (!IsValidAlignment(alignment))
    return cpp::fail(Failure::InvalidAlignment);

  std::size_t total_bytes_needed = AlignUp(bytes_needed, alignment);
  std::size_t new_block_size = block->size - total_bytes_needed;

  // Minimum size for a new block.
  if (new_block_size < AlignUp(GetBlockHeaderSize() + 1, alignment))
    return nullptr;

  ZeroBlock(block);
  std::byte* new_block_addr = AsBytePtr(block) + total_bytes_needed;
  auto* new_header = reinterpret_cast<BlockHeader*>(new_block_addr);
  new_header->next = block->next;
  new_header->size = new_block_size;

  block->size = total_bytes_needed;
  block->next = new_header;

  return new_header;
}

// Coalesces free block so long as the |next| ptr is equivalent to the
// succeeding block when using offset of |block|.
inline Failable<void> CoalesceBlock(BlockHeader* block) {
  if (!block)
    return cpp::fail(Failure::HeaderIsNullptr);

  while (AsBytePtr(block->next) == AsBytePtr(block) + block->size) {
    BlockHeader* next = block->next;
    block->size += next->size;
    block->next = next->next;
  }

  ZeroBlock(block);

  return {};
}

} // namespace dmt::allocator::internal
