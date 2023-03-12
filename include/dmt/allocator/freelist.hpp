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

    auto first_fit_or = internal::FindChunkByFirstFit(free_list_, request_size);
    if (!first_fit_or.has_value())
      return nullptr;

    auto first_fit = first_fit_or.value();
    auto new_header_or =
        internal::SplitChunk(first_fit.header, request_size, layout.alignment);

    if (new_header_or.has_value()) {
      if (first_fit.prev)
        first_fit.prev->next = new_header_or.value();

      if (first_fit.header == free_list_)
        free_list_ = new_header_or.value();

      return internal::BytePtr(first_fit.header) +
             internal::GetChunkHeaderSize();
    } else if (new_header_or == cpp::fail(internal::Error::ChunkTooSmall)) {
      if (first_fit.header == free_list_)
        free_list_ = first_fit.header->next;
      return internal::BytePtr(first_fit.header) +
             internal::GetChunkHeaderSize();
    } else {
      return nullptr;
    }
  }

  void Release(std::byte* ptr) {
    if (!ptr)
      return;

    internal::ChunkHeader* chunk = reinterpret_cast<internal::ChunkHeader*>(
        ptr - internal::GetChunkHeaderSize());
    // Zero out content
    ZeroChunk(chunk);
    chunk->next = nullptr;
    SetHeadTo(chunk);
    auto _ = dmt::internal::CoalesceChunk(chunk);
  }

private:
  // We have to explicitly provide the parent class in contexts with a
  // nondepedent name. For more information, see:
  // https://stackoverflow.com/questions/75595977/access-protected-members-of-base-class-when-using-template-parameter.
  using Parent = Chunk<Args...>;

  void SetHeadTo(internal::ChunkHeader* chunk) {
    chunk_->next = free_list_;
    free_list_ = chunk;
  }

  void ExtractChunk(internal::HeaderPair header) {
    if (header.header == free_list_) {
      free_list_ = free_list_->next;
      return;
    }

    header.prev->next = header.header->next;
    header.header->next = nullptr;
  }

  // Pointer to entire chunk of memory.
  internal::ChunkHeader* chunk_ = nullptr;

  internal::ChunkHeader* free_list_ = nullptr;
};

} // namespace dmt::allocator