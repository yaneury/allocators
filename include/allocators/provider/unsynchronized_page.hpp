#pragma once

#include <cstdint>

#include <allocators/common/error.hpp>
#include <allocators/internal/block_array.hpp>
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
    if (count == 0 || count > internal::VirtualAddressRange::kMaxPageCount)
        [[unlikely]]
      return cpp::fail(Error::InvalidInput);

    if (OutOfSpace()) {
      if (auto result = FetchNewBlockArray(); result.has_error())
        return cpp::fail(result.error());
    }

    auto va_range_or = internal::FetchPages(count);
    if (va_range_or.has_error()) [[unlikely]]
      return cpp::fail(Error::Internal);

    auto va_range = va_range_or.value();
    head_->PushBackUnchecked(va_range);

    return reinterpret_cast<std::byte*>(va_range.address);
  }

  Result<void> Return(std::byte* bytes) {
    if (bytes == nullptr) [[unlikely]]
      return cpp::fail(Error::InvalidInput);

    std::uint64_t address = reinterpret_cast<std::uint64_t>(bytes);
    auto predicate = [&](const auto& va_range) {
      return va_range.base == address;
    };

    BlockArray* itr = head_;
    while (itr != nullptr) {
      if (auto value_or = itr->Remove(predicate); value_or.has_value()) {
        (void)internal::ReturnPages(
            value_or.value()); // TODO: Don't ignore error
        return {};
      }
    }

    return cpp::fail(Error::InvalidInput);
  }

  static constexpr std::size_t GetBlockSize() {
    return internal::GetPageSize();
  }

private:
  using BlockArray =
      internal::BlockArray<GetBlockSize(), internal::VirtualAddressRange>;

  bool OutOfSpace() const {
    return head_ == nullptr || head_->GetSize() == head_->GetCapacity();
  }

  Result<void> FetchNewBlockArray() {
    auto va_range_or = internal::FetchPages(1);
    if (va_range_or.has_error())
      return cpp::fail(Error::Internal);

    auto va_range = va_range_or.value();

    BlockArray* new_block_array = internal::AsBlockArrayPtr<
        GetBlockSize(), internal::VirtualAddressRange>(va_range.address);

    new_block_array->next = head_;
    head_ = new_block_array;
  }

  BlockArray* head_ = nullptr;
};

} // namespace allocators::provider
