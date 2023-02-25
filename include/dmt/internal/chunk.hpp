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

inline constexpr std::size_t GetChunkHeaderSize() {
  return sizeof(ChunkHeader);
}

inline std::byte* GetChunk(ChunkHeader* header) {
  return reinterpret_cast<std::byte*>(header) + GetChunkHeaderSize();
}

inline ChunkHeader*
CreateChunkHeaderFromAllocation(Allocation allocation,
                                ChunkHeader* next = nullptr) {
  memset(static_cast<void*>(allocation.base), 0, allocation.size);
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

template <class T> struct Node {
  T* value = nullptr;
  Node<T>* prev = nullptr;
  Node<T>* next = nullptr;

  Node(T* value, Node<T>* prev = nullptr, Node<T>* next = nullptr)
      : value(value), prev(prev), next(next) {}
};

template <class T> inline void Extract(Node<T>* node) {
  if (!node)
    return;

  if (!node->prev)
    return;

  node->prev->next = node->next;

  if (!node->next)
    return;

  node->next->prev = node->prev;
}

template <class T> inline void Append(Node<T>* node, Node<T>* other) {
  assert(node != nullptr && other != nullptr);
  node->next = other;
  other->prev = node;
}

} // namespace dmt::internal
