#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <functional>

#include <libdmt/internal/platform.hpp>

namespace dmt::internal {

// TODO: Find a single home for Byte. It's all over the place.
using Byte = uint8_t;

struct ChunkHeader {
  std::size_t size = 0;
  ChunkHeader* next = nullptr;
};

constexpr std::size_t GetChunkHeaderSize() { return sizeof(ChunkHeader); }

Byte* GetChunk(ChunkHeader* header) {
  return reinterpret_cast<Byte*>(header) + GetChunkHeaderSize();
}

ChunkHeader* CreateChunkHeaderFromAllocation(Allocation allocation,
                                             ChunkHeader* next = nullptr) {
  memset(static_cast<void*>(allocation.base), 0, allocation.size);
  ChunkHeader* header = reinterpret_cast<ChunkHeader*>(allocation.base);
  header->size = allocation.size;
  header->next = next;
  return header;
}

void ReleaseChunks(ChunkHeader* head, std::function<void(Allocation)> release) {
  ChunkHeader* itr = head;
  while (itr != nullptr) {
    ChunkHeader* next = itr->next;
    release(
        Allocation{.base = reinterpret_cast<Byte*>(itr), .size = itr->size});

    itr = next;
  }
}

} // namespace dmt::internal
