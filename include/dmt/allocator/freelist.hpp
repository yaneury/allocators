#pragma once

#include <cstddef>
#include <dmt/allocator/chunk.hpp>
#include <dmt/allocator/trait.hpp>
#include <dmt/internal/util.hpp>

namespace dmt::allocator {

template <class... Args> class FreeList : public Chunk<Args...> {
public:
  FreeList() = default;
  ~FreeList() = default;

  std::byte* AllocateUnaligned(std::size_t size) {
    return Allocate(
        Layout{.size = size, .alignment = internal::kMinimumAlignment});
  }

  std::byte* Allocate(Layout layout) noexcept {
    if (!IsValid(layout))
      return nullptr;

    std::size_t request_size = internal::AlignUp(
        layout.size + internal::GetChunkHeaderSize(), layout.alignment);

    if (request_size > Parent::kMaxRequestSize_)
      return nullptr;

    // TODO: Add sync primitives
    if (!chunk_) {
      if (chunk_ = Parent::AllocateNewChunk(); !chunk_) {
        return nullptr;
      }

      free_list_ = chunk_;
    }

    internal::ChunkHeader* first_fit = FindFirstFit(request_size);
    std::byte* ptr = reinterpret_cast<std::byte*>(first_fit);
    return ptr + internal::GetChunkHeaderSize();
  }

  void Release(std::byte* ptr) {
    if (!ptr)
      return;

    internal::ChunkHeader* chunk = reinterpret_cast<internal::ChunkHeader*>(
        ptr - internal::GetChunkHeaderSize());
    // Zero out content
    memset(reinterpret_cast<std::byte*>(chunk) + internal::GetChunkHeaderSize(),
           0, chunk->size - internal::GetChunkHeaderSize());
    chunk->next = nullptr;
    SetHeadTo(chunk);
    Coalesce(chunk);
  }

private:
  // We have to explicitly provide the parent class in contexts with a
  // nondepedent name. For more information, see:
  // https://stackoverflow.com/questions/75595977/access-protected-members-of-base-class-when-using-template-parameter.
  using Parent = Chunk<Args...>;

  internal::ChunkHeader* FindFirstFit(std::size_t size) {
    internal::ChunkHeader* itr = free_list_;

    internal::ChunkHeader* prev = nullptr;
    while (itr) {
      if (itr->size < size) {
        prev = itr;
        itr = itr->next;
        continue;
      }

      internal::ChunkHeader* new_header = Split(itr, size);
      if (prev) {
        prev->next = new_header->next;
      }

      if (free_list_ == itr) {
        free_list_ = new_header;
      }

      itr->next = nullptr;
      return itr;
    }

    return nullptr;
  }

  internal::ChunkHeader* Split(internal::ChunkHeader* chunk, std::size_t size) {
    assert(chunk != nullptr && size > 0);

    std::size_t new_chunk_size = chunk->size - size;
    std::byte* new_chunk_addr = reinterpret_cast<std::byte*>(chunk) + size;
    memset(new_chunk_addr, 0, new_chunk_size);
    internal::ChunkHeader* new_header =
        reinterpret_cast<internal::ChunkHeader*>(new_chunk_addr);

    chunk->size = size;
    new_header->next = chunk->next;
    new_header->size = new_chunk_size;

    return new_header;
  }

  void Coalesce(internal::ChunkHeader* chunk) {
    while (reinterpret_cast<std::byte*>(chunk->next) !=
           (reinterpret_cast<std::byte*>(chunk) + chunk->size)) {
      internal::ChunkHeader* next = chunk->next;
      chunk->size += next->size;
      chunk->next = next->next;
      memset(reinterpret_cast<std::byte*>(chunk) +
                 internal::GetChunkHeaderSize(),
             0, chunk->size - internal::GetChunkHeaderSize());
    }
  }

  void SetHeadTo(internal::ChunkHeader* chunk) {
    chunk_->next = free_list_;
    free_list_ = chunk;
  }

  // Pointer to entire chunk of memory.
  internal::ChunkHeader* chunk_ = nullptr;

  internal::ChunkHeader* free_list_ = nullptr;
};

} // namespace dmt::allocator