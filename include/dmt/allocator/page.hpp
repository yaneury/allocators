#pragma once

#include <atomic>
#include <cstdint>
#include <optional>
#include <thread>

#include <template/parameters.hpp>

#include "error.hpp"
#include "internal/platform.hpp"
#include "parameters.hpp"
#include "trait.hpp"

namespace dmt::allocator {

// Coarse-grained allocator that allocates multiples of system page size
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
  // Number of pages used for page registry. Defaults to 1. Should be template
  // param though.
  // TODO: Make this user-provided template param.
  static constexpr std::size_t kRegistrySize = 1;

  Page() = default;

  Result<std::byte*> Allocate(std::size_t count) {
    if (count == 0 || count >= 1 << 16)
      return cpp::fail(Error::InvalidInput);

    auto va_range_or = internal::FetchPages(count);
    if (va_range_or.has_error())
      return cpp::fail(Error::Internal);

    auto va_range = va_range_or.value();

    Span span = {.address = reinterpret_cast<std::uint64_t>(va_range.base),
                 .count = static_cast<std::uint64_t>(count)};

    if (auto result = RegisterSpan(span); result.has_error()) {
      if (auto inner_result = internal::ReturnPages(va_range);
          inner_result.has_error())
        return cpp::fail(Error::Internal);

      return cpp::fail(result.error());
    }

    return va_range.base;
  }

  Result<void> Release(std::byte* p) {
    if (p == nullptr)
      return cpp::fail(Error::InvalidInput);

    auto maybe_span = FindSpan(registry_.load(std::memory_order_relaxed), p);
    if (!maybe_span.has_value())
      return cpp::fail(Error::InvalidInput);

    auto span = maybe_span.value();

    assert(span.address == reinterpret_cast<std::uint64_t>(p));
    auto va_range =
        internal::VirtualAddressRange(/*base=*/p, /*pages=*/span.count);

    if (auto result = internal::ReturnPages(va_range); result.has_error())
      return cpp::fail(Error::Internal);

    return {};
  }

  [[nodiscard]] constexpr std::size_t GetBlockSize() const {
    return internal::GetPageSize();
  }

private:
  struct Span {
    // 48 bits tracking actual address of page Span. All modern OSes,
    // as far as I know, only support 48-bit virtual address space (about
    // 256TB!) for userspace memory. The rest is either reserved or unused.
    // Given that, we can use the remaining bits
    std::uint64_t address : 48;

    // Number of pages allocated for this Span.
    std::uint64_t count : 16;
  };

  enum class State : std::uint64_t {
    Inactive = 0,
    Empty = 1,
    Partial = 2,
    Full = 3
  };

  // Registry must be aligned on a double-word boundary to ensure it works with
  // double-word atomic instructions, hence: alignas(16).
  struct alignas(16) Registry {
    std::uint64_t span_set_address : 48;
    std::uint64_t next_slot : 12;
    std::uint64_t next_registry : 48;
    std::uint64_t state : 2;
    std::uint64_t _padding : 18;

    // Pack together to ensure sizeof(Registry) is 16 bytes.
  } __attribute__((packed));

  static_assert(sizeof(Registry) == sizeof(std::uintptr_t) * 2,
                "Registry is not size of double word");

  Result<void> RegisterSpan(Span span) {
    while (true) {
      auto registry = registry_.load(std::memory_order_relaxed);

      if (registry.state == internal::to_underlying(State::Inactive)) {
        // Initialize registry
        continue;
      }

      if (registry.next_slot == kRegistrySize) {
        auto new_registry = registry;
        new_registry.state = internal::to_underlying(State::Full);
        registry_.compare_exchange_weak(registry, new_registry);
        continue;
      }

      if (registry.state == internal::to_underlying(State::Full)) {
        // Add new registry
        continue;
      }

      Span* span_set = reinterpret_cast<Span*>(registry.span_set_address);
      auto new_registry = registry;
      ++new_registry.next_slot;

      if (registry_.compare_exchange_weak(registry, new_registry,
                                          std::memory_order_relaxed)) {
        span_set[registry.next_slot] = span;
        return {};
      }
    }
  }

  std::optional<Span> FindSpan(Registry registry, std::byte* base) {
    Span* span_set = reinterpret_cast<Span*>(registry.span_set_address);

    for (std::size_t i = sizeof(Registry); i < kRegistrySize; ++i)
      if (span_set[i].address == reinterpret_cast<std::uintptr_t>(base))
        return span_set[i];

    // TODO: Search through entire list of Registry

    return std::nullopt;
  }

  std::atomic<Registry> registry_;
};

} // namespace dmt::allocator
