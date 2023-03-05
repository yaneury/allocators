// The ChunkHeader class and associated functions.
// A chunk is block of memory allocated by a memory allocator.
// The bulk of the bytes in the block are reserved for direct
// usage by the user requesting the memory. A small portion is
// reserved in the beginning of the chunk to contain necessary
// metadata for tracking chunks. This metadata is encapsulated
// in the ChunkHeader class.

#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <functional>

#include <dmt/internal/platform.hpp>

namespace dmt::internal {

// A ChunkHeader contains the necessary metadata used to track
// a chunk of memory. The chunk is the bytes starting from
// the address of |ChunkHeader*| up to and including the
// size of the chunk, stored in the |size| field.
struct ChunkHeader {
  // The size of the entire chunk. The size includes the portion
  // of the chunk used up by this header.
  std::size_t size = 0;

  // The next chunk in the list of chunks. Chunks are kept in a
  // standard singly-linked list.
  ChunkHeader* next = nullptr;
};

// A pair of ChunkHeader* where the |prev| is guaranteed to have its |next|
// field set to |header|.
struct HeaderPair {
  ChunkHeader* prev = nullptr;
  ChunkHeader* header = nullptr;

  explicit HeaderPair(ChunkHeader* header, ChunkHeader* prev = nullptr)
      : header(header), prev(prev) {}
};

inline constexpr std::size_t GetChunkHeaderSize() {
  return sizeof(ChunkHeader);
}

inline std::byte* GetChunk(ChunkHeader* header) {
  return reinterpret_cast<std::byte*>(header) + GetChunkHeaderSize();
}

inline void ZeroChunk(ChunkHeader* header) {
  std::byte* base = reinterpret_cast<std::byte*>(header) + GetChunkHeaderSize();
  std::size_t size = header->size - GetChunkHeaderSize();
  bzero(base, size);
}

inline ChunkHeader*
CreateChunkHeaderFromAllocation(Allocation allocation,
                                ChunkHeader* next = nullptr) {
  bzero(static_cast<void*>(allocation.base), allocation.size);
  ChunkHeader* header = reinterpret_cast<ChunkHeader*>(allocation.base);
  header->size = allocation.size;
  header->next = next;
  return header;
}

inline void ReleaseChunks(ChunkHeader* head,
                          std::function<void(Allocation)> release) {
  ChunkHeader* itr = head;
  while (itr != nullptr) {
    ChunkHeader* next = itr->next;
    release(Allocation{.base = reinterpret_cast<std::byte*>(itr),
                       .size = itr->size});

    itr = next;
  }
}

inline std::optional<HeaderPair> FindChunkByFirstFit(ChunkHeader* head,
                                                     std::size_t minimum_size) {
  if (!head || minimum_size == 0)
    return std::nullopt;

  ChunkHeader* itr = head;
  ChunkHeader* prev = nullptr;

  while (itr) {
    if (itr->size < minimum_size) {
      prev = itr;
      itr = itr->next;
      continue;
    }

    return HeaderPair(itr, prev);
  }

  return std::nullopt;
}

inline std::optional<HeaderPair>
FindChunkByFit(ChunkHeader* head, std::size_t minimum_size,
               std::size_t default_fit_size,
               std::function<bool(std::size_t, std::size_t)> cmp) {
  if (!head || minimum_size == 0)
    return std::nullopt;

  ChunkHeader* itr = head;
  ChunkHeader* prev = nullptr;

  ChunkHeader* fit_header = itr;
  auto fit_size = default_fit_size;
  while (itr) {
    if (itr->size >= minimum_size && cmp(itr->size, fit_size)) {
      fit_size = itr->size;
      fit_header = itr;
    }

    prev = itr;
    itr = itr->next;
  }

  if (fit_size == default_fit_size)
    return std::nullopt;

  return HeaderPair(fit_header, prev);
}

inline std::optional<HeaderPair> FindChunkByBestFit(ChunkHeader* head,
                                                    std::size_t minimum_size) {
  return FindChunkByFit(
      head, minimum_size,
      /*default_fit_size=*/std::numeric_limits<std::size_t>::max(),
      /*cmp=*/[](std::size_t a, std::size_t b) { return a < b; });
}

inline std::optional<HeaderPair> FindChunkByWorstFit(ChunkHeader* head,
                                                     std::size_t minimum_size) {
  return FindChunkByFit(
      head, minimum_size, /*default_fit_size=*/0,
      /*cmp=*/[](std::size_t a, std::size_t b) { return a > b; });
}

// TODO: Use cpp::result instead.
inline std::optional<ChunkHeader*> SplitChunk(ChunkHeader* chunk,
                                              std::size_t bytes_needed) {
  // TODO: We should distinguish error cases here.
  if (!chunk || bytes_needed == 0 || chunk->size < bytes_needed)
    return std::nullopt;

  ZeroChunk(chunk);
  std::size_t new_chunk_size = chunk->size - bytes_needed;
  std::byte* new_chunk_addr =
      reinterpret_cast<std::byte*>(chunk) + bytes_needed;
  auto* new_header = reinterpret_cast<ChunkHeader*>(new_chunk_addr);
  new_header->next = chunk->next;
  new_header->size = new_chunk_size;

  chunk->size = bytes_needed;
  chunk->next = nullptr;

  return new_header;
}

inline std::byte* BytePtr(ChunkHeader* chunk) {
  return reinterpret_cast<std::byte*>(chunk);
}

inline void CoalesceChunk(ChunkHeader* chunk) {
  while (BytePtr(chunk->next) == BytePtr(chunk) + chunk->size) {
    ChunkHeader* next = chunk->next;
    chunk->size += next->size;
    chunk->next = next->next;
  }

  ZeroChunk(chunk);
}

} // namespace dmt::internal
