#pragma once

#include <cstddef>

#include <template/optional.hpp>

#include <allocators/common/error.hpp>
#include <allocators/common/trait.hpp>
#include <allocators/internal/block.hpp>
#include <allocators/internal/util.hpp>
#include <allocators/provider/page.hpp>

namespace allocators::strategy {

// Freelist allocator with tunable parameters. For reference as
// to how to configure, see "common/parameters.hpp".
template <class... Args> class FreeList {
public:
  // Allocator used to request memory defaults to unconfigured Page allocator.
  using Allocator =
      typename ntp::type<ProviderT<provider::Page<>>, Args...>::value;

  // Alignment used for the blocks requested. N.b. this is *not* the alignment
  // for individual allocation requests, of which may have different alignment
  // requirements.
  //
  // This field is optional. If not provided, will default to
  // |ALLOCATORS_ALLOCATORS_ALIGNMENT|. If provided, it must greater than
  // |ALLOCATORS_ALLOCATORS_ALIGNMENT| and be a power of two.
  static constexpr std::size_t kAlignment =
      std::max({ALLOCATORS_ALLOCATORS_ALIGNMENT,
                ntp::optional<AlignmentT<0>, Args...>::value});

  // Size of the blocks. This allocator doesn't support variable-sized blocks.
  // All blocks allocated are of the same size. N.b. that the size here will
  // *not* be the size of memory ultimately requested for blocks. This is so
  // because supplemental memory is needed for block headers and to ensure
  // alignment as specified with |kAlignment|.
  //
  // This field is optional. If not provided, will default
  // |ALLOCATORS_ALLOCATORS_SIZE|.
  static constexpr std::size_t kSize =
      ntp::optional<SizeT<ALLOCATORS_ALLOCATORS_SIZE>, Args...>::value;

  // Sizing limits placed on |kSize|.
  // If |HaveAtLeastSizeBytes| is provided, then block must have |kSize| bytes
  // available not including header size and alignment.
  // If |NoMoreThanSizeBytes| is provided, then block must not exceed |kSize|
  // bytes, including after accounting for header size and alignment.
  static constexpr bool kMustContainSizeBytesInSpace =
      ntp::optional<LimitT<ALLOCATORS_ALLOCATORS_LIMIT>, Args...>::value ==
      BlocksMust::HaveAtLeastSizeBytes;

  // Policy employed when block has no more space for pending request.
  // If |GrowStorage| is provided, then a new block will be requested;
  // if |ReturnNull| is provided, then nullptr is returned on the allocation
  // request. This does not mean that it's impossible to request more memory
  // though. It only means that the block has no more space for the requested
  // size. If a smaller size request comes along, it may be possible that the
  // block has sufficient storage for it.
  static constexpr bool kGrowWhenFull =
      ntp::optional<GrowT<ALLOCATORS_ALLOCATORS_GROW>, Args...>::value ==
      WhenFull::GrowStorage;

  static constexpr FindBy kSearchStrategy =
      ntp::optional<SearchT<ALLOCATORS_ALLOCATORS_SEARCH>, Args...>::value;

  struct Options {
    std::size_t alignment;
    std::size_t size;
    bool must_contain_size_bytes_in_space;
    bool grow_when_full;

    FindBy search_strategy;
  };

  static constexpr Options kDefaultOptions = {
      .alignment = kAlignment,
      .size = kSize,
      .must_contain_size_bytes_in_space = kMustContainSizeBytesInSpace,
      .grow_when_full = kGrowWhenFull,
      .search_strategy = kSearchStrategy,
  };

  FreeList(Options options = kDefaultOptions)
      : allocator_(Allocator()), options_(std::move(options)) {}

  FreeList(Allocator& allocator, Options options = kDefaultOptions)
      : allocator_(allocator), options_(std::move(options)) {}

  FreeList(Allocator&& allocator, Options options = kDefaultOptions)
      : allocator_(std::move(allocator)), options_(std::move(options)) {}

  template <class... AllocatorArgs>
  FreeList(AllocatorArgs&&... args)
      : allocator_(std::forward<AllocatorArgs>(args)...),
        options_(kDefaultOptions) {}

  Result<std::byte*> Find(Layout layout) noexcept {
    if (!IsValid(layout))
      return cpp::fail(Error::InvalidInput);

    std::size_t request_size = internal::AlignUp(
        layout.size + internal::GetBlockHeaderSize(), layout.alignment);

    if (request_size > GetMaxRequestSize())
      return cpp::fail(Error::SizeRequestTooLarge);

    if (auto init = InitBlockIfUnset(); init.has_error())
      return cpp::fail(init.error());

    if constexpr (kGrowWhenFull == WhenFull::ReturnNull)
      if (free_list_ == nullptr)
        return cpp::fail(Error::NoFreeBlock);

    internal::Failable<std::optional<internal::HeaderPair>> first_fit_or_error =
        GetFindBlockFn()(free_list_, request_size);
    if (first_fit_or_error.has_error())
      return cpp::fail(Error::Internal);

    std::optional<internal::HeaderPair> first_fit_or =
        first_fit_or_error.value();
    if (!first_fit_or.has_value())
      return cpp::fail(Error::NoFreeBlock);

    auto first_fit = first_fit_or.value();
    auto new_header_or =
        internal::SplitBlock(first_fit.header, request_size, layout.alignment);

    // TODO: This should never occur. Also, if it is in fact possible,
    // we should log the error before dropping it on the ground.
    if (new_header_or.has_error())
      return cpp::fail(Error::Internal);

    auto new_header = new_header_or.value();
    if (first_fit.header == free_list_)
      free_list_ = new_header;
    else if (first_fit.prev)
      first_fit.prev->next = new_header;

    first_fit.header->next = nullptr;
    return internal::AsBytePtr(first_fit.header) +
           internal::GetBlockHeaderSize();
  }

  Result<std::byte*> Find(std::size_t size) noexcept {
    return Find(Layout(size, internal::kMinimumAlignment));
  }

  Result<void> Return(std::byte* ptr) {
    if (ptr == nullptr)
      return cpp::fail(Error::InvalidInput);

    std::byte* low = reinterpret_cast<std::byte*>(block_);
    std::byte* high = reinterpret_cast<std::byte*>(block_) + block_->size;
    if (ptr < low || ptr > high)
      return cpp::fail(Error::InvalidInput);

    auto block = internal::GetHeader(ptr);
    if (!free_list_) {
      // TODO: Should we zero out the content here?
      free_list_ = block;
      return {};
    }

    auto prior_or = internal::FindPriorBlock(free_list_, block);
    // TODO: Add better error here. When will this happen?
    if (prior_or.has_error())
      return {};

    auto prior = prior_or.value();
    if (prior) {
      auto next = prior->next;
      prior->next = block;
      block->next = next;
      if (auto result = internal::CoalesceBlock(prior); result.has_error())
        return cpp::fail(Error::Internal);
    } else {
      block->next = free_list_;
      free_list_ = block;
      if (auto result = internal::CoalesceBlock(free_list_); result.has_error())
        return cpp::fail(Error::Internal);
    }

    if (free_list_->size == GetAlignedSize()) {
      // TODO: Add error handling.
      (void)ReleaseAllBlocks(block_);
      // free_list_ = block_ = nullptr;
    }

    return {};
  }

  Allocator& GetAllocator() { return allocator_; }

  constexpr bool AcceptsAlignment() const { return true; }

  constexpr bool AcceptsReturn() const { return false; }

private:
  // Ultimate size of the blocks after accounting for header and alignment.
  [[nodiscard]] constexpr std::size_t GetAlignedSize() const {
    return options_.must_contain_size_bytes_in_space
               ? internal::AlignUp(options_.size +
                                       internal::GetBlockHeaderSize(),
                                   options_.alignment)
               : internal::AlignDown(options_.size, options_.alignment);
  }

  internal::VirtualAddressRange CreateAllocation(std::byte* base) {
    return internal::VirtualAddressRange(base, GetAlignedSize());
  }

  Result<internal::BlockHeader*>
  AllocateNewBlock(internal::BlockHeader* next = nullptr) {
    Result<std::byte*> base_or = allocator_.Provide(GetAlignedSize());

    if (base_or.has_error())
      return cpp::fail(base_or.error());

    auto allocation =
        internal::VirtualAddressRange(base_or.value(), GetAlignedSize());
    return internal::BlockHeader::Create(allocation, next);
  }

  Result<void> ReleaseBlock(internal::BlockHeader* block) { return {}; }

  Result<void> ReleaseAllBlocks(internal::BlockHeader* block,
                                internal::BlockHeader* sentinel = nullptr) {
    auto release = [&](std::byte* p) -> internal::Failable<void> {
      auto result = allocator_.Return(p);
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

  // Max size allowed per request.
  constexpr std::size_t GetMaxRequestSize() { return GetAlignedSize(); }

  auto GetFindBlockFn() {
    return options_.search_strategy == FindBy::FirstFit
               ? internal::FindBlockByFirstFit
           : options_.search_strategy == FindBy::BestFit
               ? internal::FindBlockByBestFit
               : internal::FindBlockByWorstFit;
  }

  // TODO: Make this thread safe.
  Result<void> InitBlockIfUnset() {
    if (block_)
      return {};

    auto new_block_or = AllocateNewBlock();
    if (new_block_or.has_error())
      return cpp::fail(new_block_or.error());

    block_ = new_block_or.value();
    free_list_ = internal::PtrAdd(block_, internal::GetBlockHeaderSize());
    free_list_->next = nullptr;
    free_list_->size = block_->size - internal::GetBlockHeaderSize();
    return {};
  }

  Allocator allocator_;
  Options options_;
  internal::BlockHeader* block_ = nullptr;
  internal::BlockHeader* free_list_ = nullptr;
};

} // namespace allocators::strategy
