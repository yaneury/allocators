#include <stack>

#include <catch2/catch_all.hpp>

#include <dmt/allocator/bump.hpp>
#include <dmt/allocator/fixed.hpp>
#include <dmt/allocator/freelist.hpp>
#include <dmt/allocator/page.hpp>

#include "util.hpp"

using namespace dmt::allocator;

template <class... Allocator> struct AllocatorPack {};

using AllocatorsUnderTest = AllocatorPack<Bump<>, FreeList<>>;

TEMPLATE_LIST_TEST_CASE("Default allocators", "[allocator][all][benchmarks]",
                        AllocatorsUnderTest) {
  // Use page-sized blocks for every allocator.
  static constexpr std::size_t kBlockSize = 4096;
  static constexpr std::array kRequestSizes = {
      1ul, 2ul, 4ul, 8ul, 16ul, 32ul, 64ul, 128ul, 256ul, 1024ul, 2048ul};

  using Allocator = TestType;

  auto make_allocations = []() {
    Allocator allocator;
    std::stack<std::byte*> allocations;
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
  };

  BENCHMARK("Allocate and Release variable-sized objects on LIFO basis") {
    return make_allocations();
  };
}
