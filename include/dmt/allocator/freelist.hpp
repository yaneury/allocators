#pragma once

#include <cstddef>
#include <dmt/allocator/chunk.hpp>
#include <dmt/allocator/error.hpp>
#include <dmt/allocator/trait.hpp>
#include <dmt/internal/util.hpp>

namespace dmt::allocator {

template <class... Args> class FreeList : public Chunk<Args...> {
public:
  FreeList() = default;
  ~FreeList() = default;

  Result<std::byte*> AllocateUnaligned(std::size_t size) noexcept {
    return Allocate(
        Layout{.size = size, .alignment = internal::kMinimumAlignment});
  }

  Result<std::byte*> Allocate(Layout layout) noexcept {
    if (!IsValid(layout))
      return cpp::fail(Error::InvalidInput);

    std::size_t request_size = internal::AlignUp(
        layout.size + internal::GetChunkHeaderSize(), layout.alignment);

    if (request_size > kMaxRequestSize_)
      return cpp::fail(Error::SizeRequestTooLarge);

    // TODO: Add sync primitives
    if (!chunk_) {
      if (chunk_ = Parent::AllocateNewChunk(); !chunk_)
        return cpp::fail(Error::OutOfMemory);

      free_list_ = chunk_;
    }

    auto first_fit_or = internal::FindChunkByFirstFit(free_list_, request_size);
    if (!first_fit_or.has_value())
      return cpp::fail(Error::NoFreeChunk);

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
    } else if (new_header_or == cpp::fail(internal::Failure::ChunkTooSmall)) {
      if (first_fit.header == free_list_)
        free_list_ = first_fit.header->next;
      return internal::BytePtr(first_fit.header) +
             internal::GetChunkHeaderSize();
    } else {
      return cpp::fail(Error::NoFreeChunk);
    }
  }

  Result<void> Release(std::byte* ptr) {
    if (!ptr)
      return cpp::fail(Error::InvalidInput);

    internal::ChunkHeader* chunk = internal::GetHeader(ptr);
    if (!free_list_) {
      free_list_ = chunk;
      return {};
    }

    auto prior_or = internal::FindPriorChunk(free_list_, chunk);
    // TODO: Add better error here. When will this happen?
    if (prior_or.has_error())
      return {};

    internal::ChunkHeader* prior = prior_or.value();
    if (prior == nullptr) {
      chunk->next = free_list_;
      free_list_ = chunk;
    } else {
      internal::ChunkHeader* next = prior->next;
      prior->next = chunk;
      chunk->next = next;
    }

    auto _ = internal::CoalesceChunk(prior);

    return {};
  }

private:
  // We have to explicitly provide the parent class in contexts with a
  // nondepedent name. For more information, see:
  // https://stackoverflow.com/questions/75595977/access-protected-members-of-base-class-when-using-template-parameter.
  using Parent = Chunk<Args...>;

  // Max size allowed per request.
  static constexpr std::size_t kMaxRequestSize_ = Parent::kAlignedSize_;

  // Pointer to entire chunk of memory.
  internal::ChunkHeader* chunk_ = nullptr;

  internal::ChunkHeader* free_list_ = nullptr;
};

} // namespace dmt::allocator