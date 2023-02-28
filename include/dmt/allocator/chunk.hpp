#pragma once

#include <dmt/allocator/parameters.hpp>
#include <dmt/allocator/trait.hpp>
#include <dmt/internal/util.hpp>
#include <template/parameters.hpp>

namespace dmt::allocator {

template <class... Args> class Chunk {
public:
  // Alignment used for the chunks requested. N.b. this is *not* the alignment
  // for individual allocation requests, of which may have different alignment
  // requirements.
  //
  // This field is optional. If not provided, will default to |sizeof(void*)|.
  // If provided, it must greater than |sizeof(void*)| and be a power of two.
  static constexpr std::size_t kAlignment =
      std::max({sizeof(void*), ntp::optional<AlignmentT<0>, Args...>::value});

  // Size of the chunks. This allocator doesn't support variable-sized chunks.
  // All chunks allocated are of the same size. N.b. that the size here will
  // *not* be the size of memory ultimately requested for chunks. This is so
  // because supplemental memory is needed for chunk headers and to ensure
  // alignment as specified with |kAlignment|.
  //
  // This field is optional. If not provided, will default to the common page
  // size, 4096.
  static constexpr std::size_t kSize =
      ntp::optional<SizeT<4096>, Args...>::value;

  // Sizing limits placed on |kSize|.
  // If |HaveAtLeastSizeBytes| is provided, then chunk must have |kSize| bytes
  // available not including header size and alignment.
  // If |NoMoreThanSizeBytes| is provided, then chunk must not exceed |kSize|
  // bytes, including after accounting for header size and alignment.
  static constexpr bool kMustContainSizeBytesInSpace =
      ntp::optional<LimitT<ChunksMust::HaveAtLeastSizeBytes>, Args...>::value ==
      ChunksMust::HaveAtLeastSizeBytes;

  // Policy employed when chunk has no more space for pending request.
  // If |GrowStorage| is provided, then a new chunk will be requested;
  // if |ReturnNull| is provided, then nullptr is returned on the allocation
  // request. This does not mean that it's impossible to request more memory
  // though. It only means that the chunk has no more space for the requested
  // size. If a smaller size request comes along, it may be possible that the
  // chunk has sufficient storage for it.
  static constexpr bool kGrowWhenFull =
      ntp::optional<GrowT<WhenFull::GrowStorage>, Args...>::value ==
      WhenFull::GrowStorage;

protected:
  // Ultimate size of the chunks after accounting for header and alignment.
  static constexpr std::size_t kAlignedSize_ =
      kMustContainSizeBytesInSpace
          ? internal::AlignUp(kSize + internal::GetChunkHeaderSize(),
                              kAlignment)
          : internal::AlignUp(kSize - internal::GetChunkHeaderSize(),
                              kAlignment);

  // Max size allowed per request when accounting for aligned size and chunk
  // header.
  static constexpr std::size_t kMaxRequestSize_ =
      kAlignedSize_ - internal::GetChunkHeaderSize();

  static internal::Allocation CreateAllocation(std::byte* base) {
    std::size_t size = IsPageMultiple()
                           ? kAlignedSize_ / internal::GetPageSize()
                           : kAlignedSize_;
    return internal::Allocation{.base = static_cast<std::byte*>(base),
                                .size = size};
  }

  static bool IsPageMultiple() {
    static const auto page_size = internal::GetPageSize();
    return kAlignedSize_ >= page_size && kAlignedSize_ % page_size == 0;
  }

  static dmt::internal::ChunkHeader* AllocateNewChunk() {
    auto allocation =
        IsPageMultiple()
            ? internal::AllocatePages(kAlignedSize_ / internal::GetPageSize())
            : internal::AllocateBytes(kAlignedSize_, kAlignment);

    if (!allocation.has_value())
      return nullptr;

    return dmt::internal::CreateChunkHeaderFromAllocation(allocation.value());
  }

  static void ReleaseChunks(dmt::internal::ChunkHeader* chunk) {
    auto release =
        IsPageMultiple() ? internal::ReleasePages : internal::ReleaseBytes;
    dmt::internal::ReleaseChunks(chunk, std::move(release));
  }

  // Various assertions hidden from user API but added here to ensure invariants
  // are met at compile time.
  static_assert(internal::IsPowerOfTwo(kAlignment),
                "kAlignment must be a power of 2.");
};

} // namespace dmt::allocator