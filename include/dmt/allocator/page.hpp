#pragma once

#include <array>

#include <template/parameters.hpp>

#include "error.hpp"
#include "internal/platform.hpp"
#include "parameters.hpp"
#include "trait.hpp"

namespace dmt::allocator {

// Coarse-grained allocator that allocated multiples of system page size
// on request. This is used internally by other allocators in this library
// to fetch memory from the heap. However, it's available for general usage
// in the public API.
template <class... Args> class Page {
public:
  static constexpr std::size_t kMaxRequests =
      std::max({16ul, ntp::optional<RequestT<0>, Args...>::value});

  Page() {
    for (auto& r : requests_)
      r.Unset();
  }

  Result<std::byte*> Allocate(Layout layout) {
    if (!IsValid(layout))
      return cpp::fail(Error::InvalidInput);

    std::size_t index = kMaxRequests;
    for (size_t i = 0; i < kMaxRequests; ++i) {
      if (!requests_[i].IsSet()) {
        index = i;
        break;
      }
    }

    if (index == kMaxRequests)
      return cpp::fail(Error::ReachedMemoryLimit);

    std::size_t page_aligned_size =
        internal::AlignUp(layout.size, internal::GetPageSize());
    auto allocation_or = internal::AllocatePages(page_aligned_size);
    if (!allocation_or.has_value())
      return cpp::fail(Error::OutOfMemory);

    auto allocation = allocation_or.value();
    requests_[index] = allocation;

    return allocation.base;
  }

  Result<std::byte*> Allocate(std::size_t size) noexcept {
    return Allocate(Layout(size, internal::kMinimumAlignment));
  }

  Result<void> Release(std::byte* ptr) {
    if (!ptr)
      return cpp::fail(Error::InvalidInput);

    auto itr =
        std::find_if(std::begin(requests_), std::end(requests_),
                     [=](auto& allocation) { return allocation.base == ptr; });

    if (itr == std::end(requests_))
      return cpp::fail(Error::InvalidInput);

    auto result = internal::ReleasePages(*itr);
    itr->Unset();

    if (result.has_error())
      return cpp::fail(Error::Internal);

    return {};
  }

private:
  std::array<internal::Allocation, kMaxRequests> requests_;
};

} // namespace dmt::allocator
