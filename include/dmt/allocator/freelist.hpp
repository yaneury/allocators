#pragma once

#include "dmt/internal/block.hpp"
#include "template/optional.hpp"
#include <cstddef>
#include <dmt/allocator/block.hpp>
#include <dmt/allocator/error.hpp>
#include <dmt/allocator/trait.hpp>
#include <dmt/internal/util.hpp>

namespace dmt::allocator {

template <class... Args> class FreeList : public Block<Args...> {
public:
  static constexpr FindBy kSearchStrategy =
      ntp::optional<SearchT<FindBy::FirstFit>, Args...>::value;

  FreeList() = default;
  ~FreeList() = default;

  Result<std::byte*> AllocateUnaligned(std::size_t size) noexcept {
    return Allocate(
        Layout{.size = size, .alignment = internal::kMinimumAlignment});
  }

  Result<std::byte*> Allocate(Layout layout) noexcept {
    if (auto init = InitBlockIfUnset(); init.has_error())
      return cpp::fail(init.error());

    if (!IsValid(layout))
      return cpp::fail(Error::InvalidInput);

    std::size_t request_size = internal::AlignUp(
        layout.size + internal::GetBlockHeaderSize(), layout.alignment);

    if (request_size > kMaxRequestSize_)
      return cpp::fail(Error::SizeRequestTooLarge);

    auto first_fit_or = FindBlock(free_list_, request_size);
    if (!first_fit_or.has_value())
      return cpp::fail(Error::NoFreeBlock);

    auto first_fit = first_fit_or.value();
    auto new_header_or =
        internal::SplitBlock(first_fit.header, request_size, layout.alignment);

    if (new_header_or.has_value()) {
      if (first_fit.prev)
        first_fit.prev->next = new_header_or.value();

      if (first_fit.header == free_list_)
        free_list_ = new_header_or.value();

      return internal::BytePtr(first_fit.header) +
             internal::GetBlockHeaderSize();
    } else if (new_header_or == cpp::fail(internal::Failure::BlockTooSmall)) {
      if (first_fit.header == free_list_)
        free_list_ = first_fit.header->next;
      return internal::BytePtr(first_fit.header) +
             internal::GetBlockHeaderSize();
    } else {
      return cpp::fail(Error::NoFreeBlock);
    }
  }

  Result<void> Release(std::byte* ptr) {
    if (!ptr)
      return cpp::fail(Error::InvalidInput);

    internal::BlockHeader* block = internal::GetHeader(ptr);
    if (!free_list_) {
      free_list_ = block;
      return {};
    }

    auto prior_or = internal::FindPriorBlock(free_list_, block);
    // TODO: Add better error here. When will this happen?
    if (prior_or.has_error())
      return {};

    internal::BlockHeader* prior = prior_or.value();
    if (prior == nullptr) {
      block->next = free_list_;
      free_list_ = block;
    } else {
      internal::BlockHeader* next = prior->next;
      prior->next = block;
      block->next = next;
    }

    auto _ = internal::CoalesceBlock(prior);

    if (free_list_->size == Parent::kAlignedSize_) {
      Parent::ReleaseBlocks(block_);
      free_list_ = block_ = nullptr;
    }

    return {};
  }

private:
  // We have to explicitly provide the parent class in contexts with a
  // nondepedent name. For more information, see:
  // https://stackoverflow.com/questions/75595977/access-protected-members-of-base-class-when-using-template-parameter.
  using Parent = Block<Args...>;

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

    auto new_block = Parent::AllocateNewBlock();
    if (!new_block)
      return cpp::fail(Error::OutOfMemory);

    free_list_ = block_ = new_block;
    return {};
  }

  // Pointer to entire block of memory.
  internal::BlockHeader* block_ = nullptr;

  internal::BlockHeader* free_list_ = nullptr;
};

} // namespace dmt::allocator
