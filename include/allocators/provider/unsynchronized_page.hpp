#pragma once

#include <cstdint>

#include <allocators/common/error.hpp>
#include <allocators/internal/block_map.hpp>
#include <allocators/internal/util.hpp>

namespace allocators::provider {

// Provider class that returns page-aligned and page-sized blocks. The page size
// is determined by the platform, 4KB for most scenarios. For the actual page
// size used on particular platform, see |internal::GetPageSize|. This provider
// is not thread-safe.
template <class... Args> class UnsynchronizedPage {
public:
  UnsynchronizedPage() = default;
  ~UnsynchronizedPage() = default;

  ALLOCATORS_NO_COPY_NO_MOVE(UnsynchronizedPage);

  Result<std::byte*> Provide(std::size_t count) {
    // DINFO("Request for: " << count << " pages.");

    if (count == 0 || count > internal::VirtualAddressRange::kMaxPageCount)
        [[unlikely]]
      return cpp::fail(Error::InvalidInput);

    if (OutOfSpace()) {
      // DINFO("Out of space. Fetching new block array.");
      if (auto result = FetchNewBlockMap(); result.has_error())
        return cpp::fail(result.error());
    }

    auto va_range_or = internal::FetchPages(count);
    if (va_range_or.has_error()) [[unlikely]]
      return cpp::fail(Error::Internal);

    auto va_range = va_range_or.value();
    head_->Insert(va_range);

    // DINFO("Providing address : " << internal::ToBytePtr(va_range.address));
    return internal::ToBytePtr(va_range.address);
  }

  Result<void> Return(std::byte* bytes) {
    if (bytes == nullptr) [[unlikely]]
      return cpp::fail(Error::InvalidInput);

    auto address = internal::FromBytePtr<std::uint64_t>(bytes);

    BlockMap* itr = head_;
    while (itr != nullptr) {
      if (auto value_or = itr->Take(address); value_or.has_value()) {
        (void)internal::ReturnPages(
            value_or.value()); // TODO: Don't ignore error

        return {};
      }

      itr = itr->GetNext();
    }

    return cpp::fail(Error::InvalidInput);
  }

  static constexpr std::size_t GetBlockSize() {
    return internal::GetPageSize();
  }

private:
  using BlockMap = internal::BlockMap<GetBlockSize()>;

  bool OutOfSpace() const { return head_ == nullptr || head_->IsFull(); }

  Result<void> FetchNewBlockMap() {
    auto va_range_or = internal::FetchPages(1);
    if (va_range_or.has_error())
      return cpp::fail(Error::Internal);

    auto va_range = va_range_or.value();

    BlockMap* new_block_map = internal::AsBlockMapPtr<GetBlockSize()>(
        internal::ToBytePtr(va_range.address));

    new_block_map->SetNext(reinterpret_cast<std::byte*>(head_));
    head_ = new_block_map;

    return {};
  }

  BlockMap* head_ = nullptr;
};

} // namespace allocators::provider
