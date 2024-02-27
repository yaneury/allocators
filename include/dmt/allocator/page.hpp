#pragma once

#include <array>
#include <mutex>

#include <template/parameters.hpp>

#include "error.hpp"
#include "internal/common.hpp"
#include "internal/platform.hpp"
#include "parameters.hpp"
#include "trait.hpp"

namespace dmt::allocator {

// Coarse-grained allocator that allocated multiples of system page size
// on request. This is used internally by other allocators in this library
// to fetch memory from the heap. However, it's available for general usage
// in the public API.
//
// This is very limited in practice. Any non-trivial program will quickly exceed
// the maximum number of pages configured. Also consider that certain objects
// can exceed the size of a page. This structure doesn't accommodate those
// requests at all.
template <class... Args> class Page {
public:
  static constexpr std::size_t k4KBPageSize = 1 << 12;

  // Page size. This field determines the page size when requesting memory
  // from OS.
  // This field is optional. If not provided, will default to
  // |DMT_ALLOCATOR_PAGE_SIZE|. If provided, the value must be greater
  // than and a multiple of 4KB.
  static constexpr std::size_t kPageSize =
      ntp::optional<SizeT<DMT_ALLOCATOR_PAGE_SIZE>, Args...>::value;

  static_assert(kPageSize >= k4KBPageSize,
                "Provided page size is not greater than or equal to 4KB");
  static_assert(kPageSize % k4KBPageSize == 0,
                "Provided page size is not multiple of 4KB");

  // TODO: Make this growable. TCMalloc uses a Span to contain a range of
  //  contiguous pages. That sounds like a promising approach.
  static constexpr std::size_t kMaxRequests =
      std::max({1ul << internal::kSmallPageShift,
                ntp::optional<RequestT<0>, Args...>::value});

  Page() : lock(std::make_unique<std::mutex>()) {}

  Result<std::byte*> Allocate(std::size_t count) {
    if (count == 0)
      return cpp::fail(Error::InvalidInput);

    std::lock_guard<std::mutex> guard(*lock);
    std::size_t index = kMaxRequests;
    for (size_t i = 0; i < kMaxRequests; ++i) {
      if (!requests_[i].IsSet()) {
        index = i;
        break;
      }
    }

    if (index == kMaxRequests)
      return cpp::fail(Error::ReachedMemoryLimit);

    auto allocation_or = internal::FetchPages(count);
    if (!allocation_or.has_value())
      return cpp::fail(Error::OutOfMemory);

    auto allocation = allocation_or.value();
    requests_[index] = allocation;

    return allocation.base;
  }

  Result<void> Release(std::byte* ptr) {
    if (!ptr)
      return cpp::fail(Error::InvalidInput);

    // TODO: Change to lock-free algorithm
    std::lock_guard<std::mutex> guard(*lock);

    auto itr =
        std::find_if(std::begin(requests_), std::end(requests_),
                     [=](auto& allocation) { return allocation.base == ptr; });

    if (itr == std::end(requests_))
      return cpp::fail(Error::InvalidInput);

    auto result = internal::ReturnPages(*itr);
    itr->Unset();

    if (result.has_error())
      return cpp::fail(Error::Internal);

    return {};
  }

private:
  std::array<internal::Allocation, kMaxRequests> requests_;

  // Mutex is wrapped inside a std::unique_ptr in order to allow move
  // construction. However, I'm probably going to delete once I refactor
  // Block / Freelist.
  std::unique_ptr<std::mutex> lock;
};

} // namespace dmt::allocator
