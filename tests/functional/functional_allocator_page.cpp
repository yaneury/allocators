#include "catch2/catch_all.hpp"

#include <array>

#include "dmt/allocator/page.hpp"

using namespace dmt::allocator;

TEST_CASE("Page allocator", "[allocator][Page]") {
  // TODO: Enable once fixed.
  SKIP();

  static constexpr std::size_t kPageSize = 4096;
  static constexpr std::size_t kMaxPages =
      Page<>::kDefaultMaxSize / kPageSize - 1;

  // Use default allocator parameters to see upper bound of space.
  using AllocatorUnderTest = Page<CountT<kMaxPages>>;

  SECTION("Can allocate 1 * kMaxPages worth of pages") {
    std::array<std::byte*, kMaxPages> allocations = {};
    AllocatorUnderTest allocator;

    for (auto i = 0u; i < kMaxPages; ++i) {
      auto p_or = allocator.Allocate(1);
      REQUIRE(p_or.has_value());
      REQUIRE(p_or.value() != nullptr);
      allocations[i] = p_or.value();
    }

    for (auto i = 0u; i < kMaxPages; ++i) {
      REQUIRE_NOTHROW([&]() {
        std::byte* p = allocations[i];
        for (int j = 0; j < kPageSize; ++j)
          p[j] = std::byte();
      });
    }

    for (auto i = 0u; i < kMaxPages; ++i) {
      auto result = allocator.Release(allocations[i]);
      REQUIRE(result.has_value());
    }
  }

  // TODO: Support multiples pages per request.
  SECTION("Can allocator multiple pages per request") {}

  SECTION("While rejecting invalid sizes") {
    AllocatorUnderTest allocator;
    for (auto size : {0ul, kMaxPages + 1}) {
      auto p_or = allocator.Allocate(size);
      REQUIRE(p_or.has_error());
      REQUIRE(p_or.error() == Error::InvalidInput);
    }
  }
}
