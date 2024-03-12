#pragma once

#include <cstddef>
#include <functional>

#include <template/optional.hpp>

#include <allocators/common/error.hpp>
#include <allocators/common/trait.hpp>
#include <allocators/internal/block.hpp>
#include <allocators/internal/util.hpp>

namespace allocators::strategy {

struct FreeListParams {
  // Alignment used when making an allocation. Usually, allocators defer to the
  // alignment of the underlying object they are allocating. The constrains for
  // this value are that it is a power of two and greater than |sizeof(void*)|.
  template <std::size_t Alignment>
  struct AlignmentT : std::integral_constant<std::size_t, Alignment> {};

  // Policy to employ when looking for free block in free list.
  enum FindBy {
    // Use first block that contains the minimum sizes of bytes.
    FirstFit = 0,

    // Use the *smallest* block that contains the minimum sizes of bytes.
    BestFit = 1,
    //
    // Use the *largest* block that contains the minimum sizes of bytes.
    WorstFit = 2
  };

  template <FindBy FB> struct SearchT : std::integral_constant<FindBy, FB> {};
};

// Freelist allocator with tunable parameters. For reference as
// to how to configure, see "common/parameters.hpp".
template <class Provider, class... Args>
requires ProviderTrait<Provider>
class FreeList : public FreeListParams {
public:
  static constexpr std::size_t kAlignment =
      std::max({sizeof(void*), ntp::optional<AlignmentT<0>, Args...>::value});

  static constexpr FindBy kSearchStrategy =
      ntp::optional<SearchT<FindBy::BestFit>, Args...>::value;

  FreeList(Provider& provider) : provider_(provider) {}

  ALLOCATORS_NO_COPY_NO_MOVE_NO_DEFAULT(FreeList);

  Result<std::byte*> Find(Layout layout) noexcept {
    if (!IsValid(layout))
      return cpp::fail(Error::InvalidInput);

    std::size_t request_size = internal::AlignUp(
        layout.size + internal::GetBlockHeaderSize(), layout.alignment);

    if (request_size > GetAlignedSize())
      return cpp::fail(Error::SizeRequestTooLarge);

    if (auto init = InitBlockIfUnset(); init.has_error())
      return cpp::fail(init.error());

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

  constexpr bool AcceptsAlignment() const { return true; }

  constexpr bool AcceptsReturn() const { return false; }

private:
  // Ultimate size of the blocks after accounting for header and alignment.
  [[nodiscard]] static constexpr std::size_t GetAlignedSize() {
    return Provider::GetBlockSize();
  }

  internal::VirtualAddressRange CreateAllocation(std::byte* base) {
    return internal::VirtualAddressRange(base, GetAlignedSize());
  }

  Result<internal::BlockHeader*>
  AllocateNewBlock(internal::BlockHeader* next = nullptr) {
    Result<std::byte*> base_or = provider_.get().Provide(GetAlignedSize());

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
      auto result = provider_.get().Return(p);
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

  static constexpr auto GetFindBlockFn() {
    return kSearchStrategy == FindBy::FirstFit  ? internal::FindBlockByFirstFit
           : kSearchStrategy == FindBy::BestFit ? internal::FindBlockByBestFit
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

  std::reference_wrapper<Provider> provider_;

  internal::BlockHeader* block_ = nullptr;
  internal::BlockHeader* free_list_ = nullptr;
};

} // namespace allocators::strategy
