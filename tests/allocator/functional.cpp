#include <algorithm>
#include <array>
#include <queue>
#include <random>
#include <stack>
#include <vector>

#include <catch2/catch_all.hpp>

#include <dmt/allocator/bump.hpp>
#include <dmt/allocator/fixed.hpp>
#include <dmt/allocator/freelist.hpp>
#include <dmt/allocator/page.hpp>

#include "util.hpp"

using namespace dmt::allocator;

/*
 *
 *
 * Rough Contract:
 *   Allocator::Allocate(std::size_t) -> Result<std::byte*>
 *   Allocator::Release(std::byte*) -> Result<void>
 * 1. Can allocate many objects of variable sizes
 * 2. Can release all objects of variable sizes
 * 3. Input validation:
 *   a. Size > 0
 *   b. Alignment > sizeof(void) && Alignment is Power of Two
 *   c. std::byte* is not nullptr
 *   d. (Fixed sized) Size < Max
 * 4. Invalid pointer when calling Release (undefined behavior?)
 *
 *
 */

template <class... Allocator> struct AllocatorPack {};

using AllocatorsUnderTest = AllocatorPack<Bump<>, FreeList<>, Fixed<>>;

TEMPLATE_LIST_TEST_CASE("All allocators are functional",
                        "[allocator][all][functional]", AllocatorsUnderTest) {
  // Use page-sized blocks for every allocator.
  static constexpr std::size_t kBlockSize = 4096;
  static constexpr std::array kRequestSizes = {
      1ul, 2ul, 4ul, 8ul, 16ul, 32ul, 64ul, 128ul, 256ul, 1024ul, 2048ul};

  using Allocator = TestType;

  SECTION("And can allocate and release variable-sized objects on LIFO basis") {
    Allocator allocator;

    std::stack<std::byte*> allocations = {};
    for (std::size_t size : kRequestSizes)
      allocations.push(GetValueOrFail<std::byte*>(allocator.Allocate(size)));

    while (allocations.size()) {
      REQUIRE(allocator.Release(allocations.top()).has_value());
      allocations.pop();
    }
  }

  SECTION("And can allocate and release variable-sized objects FIFO basis") {
    Allocator allocator;

    std::queue<std::byte*> allocations = {};
    for (std::size_t size : kRequestSizes)
      allocations.push(GetValueOrFail<std::byte*>(allocator.Allocate(size)));

    while (allocations.size()) {
      REQUIRE(allocator.Release(allocations.front()).has_value());
      allocations.pop();
    }
  }

  SECTION(
      "And can allocate and release variable-sized objects on random basis") {
    Allocator allocator;

    std::vector<std::byte*> allocations = {};
    for (std::size_t size : kRequestSizes)
      allocations.push_back(
          GetValueOrFail<std::byte*>(allocator.Allocate(size)));

    std::random_device rd;
    std::mt19937 g(rd());
    std::shuffle(allocations.begin(), allocations.end(), g);

    for (std::byte* p : allocations)
      REQUIRE(allocator.Release(allocations.front()).has_value());
  }
}
