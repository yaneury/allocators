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
#include <result.hpp>

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

// Convenience cast function
template <class T> inline std::byte* BytePtr(T* ptr) {
  return reinterpret_cast<std::byte*>(ptr);
}

// Fixed sized of Chunk header.
inline constexpr std::size_t GetChunkHeaderSize() {
  return sizeof(ChunkHeader);
}

inline std::size_t ChunkSize(ChunkHeader* header) {
  if (!header)
    return 0;

  return header->size - GetChunkHeaderSize();
}

// Get pointer to chunk referenced by |header|.
inline std::byte* GetChunk(ChunkHeader* header) {
  return BytePtr(header) + GetChunkHeaderSize();
}

// Zero out the contents of the chunk referenced by |header|.
inline void ZeroChunk(ChunkHeader* header) {
  if (!header)
    return;

  std::byte* base = BytePtr(header) + GetChunkHeaderSize();
  std::size_t size = header->size - GetChunkHeaderSize();
  bzero(base, size);
}

// Cast |allocation| to |ChunkHeader*|.
inline ChunkHeader*
CreateChunkHeaderFromAllocation(Allocation allocation,
                                ChunkHeader* next = nullptr) {
  bzero(allocation.base, allocation.size);
  ChunkHeader* header = reinterpret_cast<ChunkHeader*>(allocation.base);
  header->size = allocation.size;
  header->next = next;
  return header;
}

// Cast |ChunkHeader*| pointed by head to |Allocation| objects and free their
// associated memory using |release|.
inline void ReleaseChunks(ChunkHeader* head,
                          std::function<void(Allocation)> release) {
  ChunkHeader* itr = head;
  while (itr != nullptr) {
    ChunkHeader* next = itr->next;
    release(Allocation{.base = BytePtr(itr), .size = itr->size});

    itr = next;
  }
}

// Return first header that has at least |minimum_size| bytes available.
inline std::optional<HeaderPair> FindChunkByFirstFit(ChunkHeader* head,
                                                     std::size_t minimum_size) {
  if (!head || minimum_size == 0)
    return std::nullopt;

  for (ChunkHeader *itr = head, *prev = nullptr; itr != nullptr;
       prev = itr, itr = itr->next)
    if (ChunkSize(itr) >= minimum_size)
      return HeaderPair(itr, prev);

  return std::nullopt;
}

// Return header that most closely fits |minimum_size| using |cmp| as the
// criteria.
inline std::optional<HeaderPair>
FindChunkByFit(ChunkHeader* head, std::size_t minimum_size,
               std::function<bool(std::size_t, std::size_t)> cmp) {
  if (!head || minimum_size == 0)
    return std::nullopt;

  std::optional<HeaderPair> target = std::nullopt;
  for (ChunkHeader *itr = head, *prev = nullptr; itr != nullptr;
       prev = itr, itr = itr->next) {
    if (ChunkSize(itr) < minimum_size)
      continue;

    if (!target.has_value() || cmp(ChunkSize(itr), ChunkSize(target->header))) {
      target = HeaderPair(itr, prev);
    }
  }
  return target;
}

inline std::optional<HeaderPair> FindChunkByBestFit(ChunkHeader* head,
                                                    std::size_t minimum_size) {
  return FindChunkByFit(
      head, minimum_size,
      /*cmp=*/[](std::size_t a, std::size_t b) { return a < b; });
}

inline std::optional<HeaderPair> FindChunkByWorstFit(ChunkHeader* head,
                                                     std::size_t minimum_size) {
  return FindChunkByFit(
      head, minimum_size,
      /*cmp=*/[](std::size_t a, std::size_t b) { return a > b; });
}

enum class Error {
  HeaderIsNullptr,
  InvalidSize,
  InvalidAlignment,
  ChunkTooSmall
};

// Split chunk and return new |ChunkHeader*|.
inline cpp::result<ChunkHeader*, Error> SplitChunk(ChunkHeader* chunk,
                                                   std::size_t bytes_needed,
                                                   std::size_t alignment) {
  if (!chunk)
    return cpp::fail(Error::HeaderIsNullptr);
  if (!bytes_needed)
    return cpp::fail(Error::InvalidSize);
  if (!IsValidAlignment(alignment))
    return cpp::fail(Error::InvalidAlignment);

  // Minimum size for a new chunk.
  std::size_t minimum_chunk_size =
      AlignUp(dmt::internal::GetChunkHeaderSize() + 1, alignment);
  std::size_t total_bytes_needed =
      AlignUp(GetChunkHeaderSize() + bytes_needed, alignment);
  std::size_t new_chunk_size = chunk->size - total_bytes_needed;

  if (new_chunk_size < minimum_chunk_size)
    return cpp::fail(Error::ChunkTooSmall);

  ZeroChunk(chunk);
  std::byte* new_chunk_addr = BytePtr(chunk) + total_bytes_needed;
  auto* new_header = reinterpret_cast<ChunkHeader*>(new_chunk_addr);
  new_header->next = chunk->next;
  new_header->size = new_chunk_size;

  chunk->size = total_bytes_needed;
  chunk->next = nullptr;

  return new_header;
}

// Coalesces free chunk so long as the |next| ptr is equivalent to actual
// succeeding chunk when using offset of |chunk|.
// TODO: This chunk can use a |next| == |nullptr| to check for occupied status.
// However, the freelist would have to be recalibrated from the beginning.
inline void CoalesceChunk(ChunkHeader* chunk) {
  while (BytePtr(chunk->next) == BytePtr(chunk) + chunk->size) {
    ChunkHeader* next = chunk->next;
    chunk->size += next->size;
    chunk->next = next->next;
  }

  ZeroChunk(chunk);
}

} // namespace dmt::internal
