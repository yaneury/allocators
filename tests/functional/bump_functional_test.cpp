#include "catch2/catch_all.hpp"

#include <ranges>
#include <vector>

#include <allocators/strategy/lockfree_bump.hpp>

#include "../util.hpp"

using namespace allocators;

using T = long;
static constexpr std::size_t SizeOfT = sizeof(T);
static constexpr std::size_t N = 10;
static constexpr std::size_t MinBlockSize = SizeOfT * N;
static constexpr std::size_t MaxBlockSize =
    MinBlockSize + internal::GetBlockHeaderSize();

template <class... Allocator> struct AllocatorPack {};

template <class... Args>
using FixedBump = strategy::LockfreeBump<GrowT<WhenFull::ReturnNull>, Args...>;

using FixedBumpAllocators = AllocatorPack<
    FixedBump<LimitT<BlocksMust::HaveAtLeastSizeBytes>, SizeT<MinBlockSize>>,
    FixedBump<LimitT<BlocksMust::NoMoreThanSizeBytes>, SizeT<MaxBlockSize>>>;

TEMPLATE_LIST_TEST_CASE("Fixed LockfreeBump allocator that can fit N objects",
                        "[functional][allocator][LockfreeBump]",
                        FixedBumpAllocators) {
  TestType allocator;

  std::array<T*, N> allocs;
  for (std::size_t i = 0; i < N; ++i)
    allocs[i] = GetPtrOrFail<T>(allocator.Find(SizeOfT));

  SECTION("All objects are neighbors to each other") {
    for (std::size_t i = 0; i < N - 1; ++i)
      REQUIRE(allocs[i] + 1 == allocs[i + 1]);
  }

  SECTION("Can not allocate more objects when at capacity") {
    REQUIRE(allocator.Find(SizeOfT) == cpp::fail(Error::ReachedMemoryLimit));
  }

  SECTION("Release is not supported") {
    REQUIRE(allocator.Return(ToBytePtr(allocs.front())) ==
            cpp::fail(Error::OperationNotSupported));
  }

  SECTION("Reset clears space") {
    REQUIRE(allocator.Reset().has_value());

    SECTION("Allowing subsequent requests") {
      for (std::size_t i = 0; i < N; ++i)
        allocs[i] = GetPtrOrFail<T>(allocator.Find(SizeOfT));
    }
  }
}

template <class... Args>
using VariableBump =
    strategy::LockfreeBump<GrowT<WhenFull::GrowStorage>, Args...>;

using VariableBumpAllocators = AllocatorPack<
    VariableBump<LimitT<BlocksMust::HaveAtLeastSizeBytes>, SizeT<MinBlockSize>>,
    VariableBump<LimitT<BlocksMust::NoMoreThanSizeBytes>, SizeT<MaxBlockSize>>>;

TEMPLATE_LIST_TEST_CASE(
    "Variable-sized LockfreeBump allocator with block size fitting N objects",
    "[functional][allocator][LockfreeBump]", VariableBumpAllocators) {
  TestType allocator;

  for (std::size_t i = 0; i < N; ++i)
    REQUIRE(allocator.Find(SizeOfT).has_value());

  SECTION("Can make allocations beyond single block") {
    // After this second loop, there should be two blocks filled with N
    // allocations each.
    for (std::size_t i = 0; i < N; ++i)
      REQUIRE(allocator.Find(SizeOfT).has_value());
  }

  SECTION("Can reset allocations") {
    REQUIRE(allocator.Reset().has_value());

    SECTION("But can't fit request size larger than block size") {
      REQUIRE(allocator.Find(MinBlockSize + 1) ==
              cpp::fail(Error::SizeRequestTooLarge));
    }
  }
}
