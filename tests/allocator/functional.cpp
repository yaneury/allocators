#include <algorithm>
#include <array>
#include <queue>
#include <random>
#include <stack>
#include <vector>

#include <catch2/catch_all.hpp>

#include "dmt/allocator/internal/util.hpp"
#include <dmt/allocator/bump.hpp>
#include <dmt/allocator/fixed.hpp>
#include <dmt/allocator/freelist.hpp>
#include <dmt/allocator/page.hpp>

#include "util.hpp"

using namespace dmt::allocator;

template <class... Allocator> struct AllocatorPack {};

using AllocatorsUnderTest = AllocatorPack<Bump<>, FreeList<>>;

TEMPLATE_LIST_TEST_CASE("All allocators are functional",
                        "[allocator][all][functional]", AllocatorsUnderTest) {
  // Use page-sized blocks for every allocator.
  static constexpr std::size_t kBlockSize = 4096;
  static constexpr std::array kRequestSizes = {
      1ul, 2ul, 4ul, 8ul, 16ul, 32ul, 64ul, 128ul, 256ul, 1024ul, 2048ul};

  using Allocator = TestType;

  SECTION("Can allocate and release variable-sized objects on LIFO basis") {
    Allocator allocator;

    std::stack<std::byte*> allocations = {};
    for (std::size_t size : kRequestSizes) {
      auto p_or = allocator.Allocate(size);
      allocations.push(GetValueOrFail<std::byte*>(p_or));
    }

    if constexpr (std::is_same_v<Allocator, Bump<>>) {
      REQUIRE(allocator.Reset().has_value());
    } else {
      while (allocations.size()) {
        REQUIRE(allocator.Release(allocations.top()).has_value());
        allocations.pop();
      }
    }
  }

  SECTION("Can allocate and release variable-sized objects FIFO basis") {
    Allocator allocator;

    std::queue<std::byte*> allocations = {};
    for (std::size_t size : kRequestSizes)
      allocations.push(GetValueOrFail<std::byte*>(allocator.Allocate(size)));

    if constexpr (std::is_same_v<Allocator, Bump<>>) {
      REQUIRE(allocator.Reset().has_value());
    } else {
      while (allocations.size()) {
        REQUIRE(allocator.Release(allocations.front()).has_value());
        allocations.pop();
      }
    }
  }

  SECTION("Can allocate and release variable-sized objects on random basis") {
    Allocator allocator;

    std::vector<std::byte*> allocations = {};
    for (std::size_t size : kRequestSizes)
      allocations.push_back(
          GetValueOrFail<std::byte*>(allocator.Allocate(size)));

    std::random_device rd;
    std::mt19937 g(rd());
    std::shuffle(allocations.begin(), allocations.end(), g);

    if constexpr (std::is_same_v<Allocator, Bump<>>) {
      REQUIRE(allocator.Reset().has_value());
    } else {
      for (std::byte* p : allocations)
        REQUIRE(allocator.Release(allocations.front()).has_value());
    }
  }

  SECTION("Rejects invalid requests") {
    Allocator allocator;

    // Invalid size.
    REQUIRE(allocator.Allocate(/*size=*/0) == cpp::fail(Error::InvalidInput));

    if constexpr (std::is_same_v<Allocator, Fixed<>>) {
      REQUIRE(allocator.Allocate(Layout(/*size=*/1, /*alignment=*/0)) ==
              cpp::fail(Error::InvalidInput));
    } else {
      // Invalid alignment.
      for (std::size_t i = 0; i < sizeof(void*); ++i)
        REQUIRE(allocator.Allocate(Layout(/*size=*/1, /*alignment=*/i)) ==
                cpp::fail(Error::InvalidInput));

      // Greater than sizeof(void*) but not power of two.
      REQUIRE(allocator.Allocate(
                  Layout(/*size=*/1, /*alignment=*/sizeof(void*) + 1)) ==
              cpp::fail(Error::InvalidInput));
    }

    // Invalid ptr.
    if constexpr (std::is_same_v<Allocator, Bump<>>)
      REQUIRE(allocator.Release(nullptr) ==
              cpp::fail(Error::OperationNotSupported));
    else
      REQUIRE(allocator.Release(nullptr) == cpp::fail(Error::InvalidInput));

    // TODO: Enable Hardening features
    // char c = 'a';
    // REQUIRE(allocator.Release(ToBytePtr(&c)) ==
    // cpp::fail(Error::InvalidInput));
  }
}
