#pragma once

#include <template/parameters.hpp>

#include "internal/block.hpp"
#include "internal/util.hpp"
#include "page.hpp"
#include "parameters.hpp"
#include "trait.hpp"

namespace dmt::allocator {

// Coarse-grained allocator that allocates fixed block sizes on request.
// This is used internally by other allocators in this library to fetch
// memory from the heap. However, it's available for general usage in the
// public API.
template <class... Args> class Block {
public:
  // Allocator used to request memory defaults to unconfigured Page allocator.
  using Allocator = typename ntp::type<AllocatorT<Page<>>, Args...>::value;

  // Alignment used for the blocks requested. N.b. this is *not* the alignment
  // for individual allocation requests, of which may have different alignment
  // requirements.
  //
  // This field is optional. If not provided, will default to
  // |DMT_ALLOCATOR_ALIGNMENT|. If provided, it must greater than
  // |DMT_ALLOCATOR_ALIGNMENT| and be a power of two.
  static constexpr std::size_t kAlignment = std::max(
      {DMT_ALLOCATOR_ALIGNMENT, ntp::optional<AlignmentT<0>, Args...>::value});

  // Size of the blocks. This allocator doesn't support variable-sized blocks.
  // All blocks allocated are of the same size. N.b. that the size here will
  // *not* be the size of memory ultimately requested for blocks. This is so
  // because supplemental memory is needed for block headers and to ensure
  // alignment as specified with |kAlignment|.
  //
  // This field is optional. If not provided, will default |DMT_ALLOCATOR_SIZE|.
  static constexpr std::size_t kSize =
      ntp::optional<SizeT<DMT_ALLOCATOR_SIZE>, Args...>::value;

  // Sizing limits placed on |kSize|.
  // If |HaveAtLeastSizeBytes| is provided, then block must have |kSize| bytes
  // available not including header size and alignment.
  // If |NoMoreThanSizeBytes| is provided, then block must not exceed |kSize|
  // bytes, including after accounting for header size and alignment.
  static constexpr bool kMustContainSizeBytesInSpace =
      ntp::optional<LimitT<DMT_ALLOCATOR_LIMIT>, Args...>::value ==
      BlocksMust::HaveAtLeastSizeBytes;

  // Policy employed when block has no more space for pending request.
  // If |GrowStorage| is provided, then a new block will be requested;
  // if |ReturnNull| is provided, then nullptr is returned on the allocation
  // request. This does not mean that it's impossible to request more memory
  // though. It only means that the block has no more space for the requested
  // size. If a smaller size request comes along, it may be possible that the
  // block has sufficient storage for it.
  static constexpr bool kGrowWhenFull =
      ntp::optional<GrowT<DMT_ALLOCATOR_GROW>, Args...>::value ==
      WhenFull::GrowStorage;

  struct Options {
    std::size_t alignment;
    std::size_t size;
    bool must_contain_size_bytes_in_space;
    bool grow_when_full;
  };

  static constexpr Options kDefaultOptions = {
      .alignment = kAlignment,
      .size = kSize,
      .must_contain_size_bytes_in_space = kMustContainSizeBytesInSpace,
      .grow_when_full = kGrowWhenFull,
  };

protected:
  Block(Options options = kDefaultOptions)
      : allocator_(Allocator()), options_(std::move(options)) {}

  Block(Allocator& allocator, Options options = kDefaultOptions)
      : allocator_(allocator), options_(std::move(options)) {}

  Block(Allocator&& allocator, Options options = kDefaultOptions)
      : allocator_(std::move(allocator)), options_(std::move(options)) {}

  // Ultimate size of the blocks after accounting for header and alignment.
  [[nodiscard]] constexpr std::size_t GetAlignedSize() const {
    return options_.must_contain_size_bytes_in_space
               ? internal::AlignUp(options_.size +
                                       internal::GetBlockHeaderSize(),
                                   options_.alignment)
               : internal::AlignDown(options_.size, options_.alignment);
  }

  internal::Allocation CreateAllocation(std::byte* base) {
    return internal::Allocation(base, GetAlignedSize());
  }

  Result<internal::BlockHeader*>
  AllocateNewBlock(internal::BlockHeader* next = nullptr) {
    Result<std::byte*> base_or = allocator_.Allocate(GetAlignedSize());

    if (base_or.has_error())
      return cpp::fail(base_or.error());

    auto allocation = internal::Allocation(base_or.value(), GetAlignedSize());
    return internal::BlockHeader::Create(allocation, next);
  }

  Result<void> ReleaseBlock(internal::BlockHeader* block) { return {}; }

  Result<void> ReleaseAllBlocks(internal::BlockHeader* block,
                                internal::BlockHeader* sentinel = nullptr) {
    auto release = [&](std::byte* p) -> internal::Failable<void> {
      auto result = allocator_.Release(p);
      if (result.has_error()) {
        DERROR("Block release failed: " << (int)result.error());
        return cpp::fail(internal::Failure::ReleaseFailed);
      }

      return {};
    };
    if (auto result =
            internal::ReleaseBlockList(block, std::move(release), sentinel);
        result.has_error())
      return cpp::fail(Error::Internal);

    return {};
  }

  // Various assertions hidden from user API but added here to ensure invariants
  // are met at compile time.
  static_assert(internal::IsPowerOfTwo(kAlignment),
                "kAlignment must be a power of 2.");

protected:
  Allocator allocator_;
  Options options_;
};

} // namespace dmt::allocator
