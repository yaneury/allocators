#pragma once

#include <cstddef>

#include <template/optional.hpp>

#include <allocators/error.hpp>
#include <allocators/internal/block.hpp>
#include <allocators/internal/util.hpp>
#include <allocators/object/block.hpp>
#include <allocators/trait.hpp>

namespace allocators {

// Freelist allocator with tunable parameters. For reference as
// to how to configure, see "parameters.hpp".
template <class... Args> class FreeList : public Block<Args...> {
public:
  // We have to explicitly provide the parent class in contexts with a
  // nondepedent name. For more information, see:
  // https://stackoverflow.com/questions/75595977/access-protected-members-of-base-class-when-using-template-parameter.
  using Parent = Block<Args...>;

  using Allocator = typename Parent::Allocator;

  static constexpr FindBy kSearchStrategy =
      ntp::optional<SearchT<ALLOCATORS_ALLOCATORS_SEARCH>, Args...>::value;

  using BlockOptions = typename Parent::Options;

  struct Options {
    // Inherited from Block::Options. Consult |Block| for documentation.
    std::size_t alignment;
    std::size_t size;
    bool must_contain_size_bytes_in_space;
    bool grow_when_full;

    FindBy search_strategy;
  };

  static BlockOptions ToBlockOptions(Options options) {
    return BlockOptions{
        .alignment = options.alignment,
        .size = options.size,
        .must_contain_size_bytes_in_space =
            options.must_contain_size_bytes_in_space,
        .grow_when_full = options.grow_when_full,
    };
  }

  static constexpr Options kDefaultOptions = {
      .alignment = Parent::kAlignment,
      .size = Parent::kSize,
      .must_contain_size_bytes_in_space = Parent::kMustContainSizeBytesInSpace,
      .grow_when_full = Parent::kGrowWhenFull,
      .search_strategy = kSearchStrategy,
  };

  // TODO: Add constructor (or factory method) that allows forwarding args
  // and setting options.
  template <class... AllocatorArgs>
  FreeList(AllocatorArgs&&... args)
      : Parent(Allocator(std::forward<AllocatorArgs>(args)...),
               ToBlockOptions(kDefaultOptions)),
        search_strategy_(kDefaultOptions.search_strategy) {}

  FreeList(Options options = kDefaultOptions)
      : Parent(Allocator(), ToBlockOptions(options)),
        search_strategy_(options.search_strategy) {}

  FreeList(Allocator& allocator, Options options = kDefaultOptions)
      : Parent(allocator, ToBlockOptions(options)),
        search_strategy_(options.search_strategy) {}

  /*
  FreeList(Allocator&& allocator, Options options = kDefaultOptions)
      : Parent(std::move(allocator), ToBlockOptions(options)),
        search_strategy_(options.search_strategy) {}
        */

  Result<std::byte*> Allocate(Layout layout) noexcept {
    if (!IsValid(layout))
      return cpp::fail(Error::InvalidInput);

    std::size_t request_size = internal::AlignUp(
        layout.size + internal::GetBlockHeaderSize(), layout.alignment);

    if (request_size > GetMaxRequestSize())
      return cpp::fail(Error::SizeRequestTooLarge);

    if (auto init = InitBlockIfUnset(); init.has_error())
      return cpp::fail(init.error());

    if constexpr (Parent::kGrowWhenFull == WhenFull::ReturnNull)
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

  Result<std::byte*> Allocate(std::size_t size) noexcept {
    return Allocate(Layout(size, internal::kMinimumAlignment));
  }

  Result<void> Release(std::byte* ptr) {
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

    if (free_list_->size == Parent::GetAlignedSize()) {
      // TODO: Add error handling.
      (void)Parent::ReleaseAllBlocks(block_);
      // free_list_ = block_ = nullptr;
    }

    return {};
  }

  Allocator& GetAllocator() { return Parent::allocator_; }

private:
  // Max size allowed per request.
  constexpr std::size_t GetMaxRequestSize() { return Parent::GetAlignedSize(); }

  auto GetFindBlockFn() {
    return search_strategy_ == FindBy::FirstFit ? internal::FindBlockByFirstFit
           : search_strategy_ == FindBy::BestFit
               ? internal::FindBlockByBestFit
               : internal::FindBlockByWorstFit;
  }

  // TODO: Make this thread safe.
  Result<void> InitBlockIfUnset() {
    if (block_)
      return {};

    auto new_block_or = Parent::AllocateNewBlock();
    if (new_block_or.has_error())
      return cpp::fail(new_block_or.error());

    block_ = new_block_or.value();
    free_list_ = internal::PtrAdd(block_, internal::GetBlockHeaderSize());
    free_list_->next = nullptr;
    free_list_->size = block_->size - internal::GetBlockHeaderSize();
    return {};
  }

  FindBy search_strategy_;

  internal::BlockHeader* block_ = nullptr;
  internal::BlockHeader* free_list_ = nullptr;
};

} // namespace allocators
