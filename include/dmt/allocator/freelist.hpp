#pragma once

#include <cassert>
#include <cstddef>

#include <template/optional.hpp>

#include "block.hpp"
#include "error.hpp"
#include "internal/block.hpp"
#include "internal/util.hpp"
#include "trait.hpp"

namespace dmt::allocator {

template <class... Args> class FreeList : public Block<Args...> {
public:
  // We have to explicitly provide the parent class in contexts with a
  // nondepedent name. For more information, see:
  // https://stackoverflow.com/questions/75595977/access-protected-members-of-base-class-when-using-template-parameter.
  using Parent = Block<Args...>;

  using Allocator = typename Parent::Allocator;

  FreeList() : Parent(Allocator()) {}

  FreeList(Allocator& allocator) : Parent(allocator) {}

  FreeList(Allocator&& allocator) : Parent(std::move(allocator)) {}

  static constexpr FindBy kSearchStrategy =
      ntp::optional<SearchT<FindBy::FirstFit>, Args...>::value;

  Result<std::byte*> Allocate(Layout layout) noexcept {
    if (!IsValid(layout))
      return cpp::fail(Error::InvalidInput);

    std::size_t request_size = internal::AlignUp(
        layout.size + internal::GetBlockHeaderSize(), layout.alignment);

    if (request_size > kMaxRequestSize_)
      return cpp::fail(Error::SizeRequestTooLarge);

    if (auto init = InitBlockIfUnset(); init.has_error())
      return cpp::fail(init.error());

    internal::Failable<std::optional<internal::HeaderPair>> first_fit_or_error =
        FindBlock(free_list_, request_size);
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
    if (!ptr)
      return cpp::fail(Error::InvalidInput);

    auto block = internal::GetHeader(ptr);
    if (!free_list_) {
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

    if (free_list_->size == Parent::kAlignedSize_) {
      // TODO: Add error handling.
      (void)Parent::ReleaseAllBlocks(block_);
      free_list_ = block_ = nullptr;
    }

    return {};
  }

private:
  // Max size allowed per request.
  static constexpr std::size_t kMaxRequestSize_ = Parent::kAlignedSize_;

  static constexpr auto FindBlock =
      kSearchStrategy == FindBy::FirstFit  ? internal::FindBlockByFirstFit
      : kSearchStrategy == FindBy::BestFit ? internal::FindBlockByBestFit
                                           : internal::FindBlockByWorstFit;

  // TODO: Make this thread safe.
  Result<void> InitBlockIfUnset() {
    if (block_)
      return {};

    auto new_block_or = Parent::AllocateNewBlock();
    if (new_block_or.has_error())
      return cpp::fail(new_block_or.error());

    auto new_block = new_block_or.value();
    free_list_ = block_ = new_block;
    return {};
  }

  Allocator allocator_;

  internal::BlockHeader* block_ = nullptr;
  internal::BlockHeader* free_list_ = nullptr;
};

} // namespace dmt::allocator
