#include <array>
#include <catch2/catch_all.hpp>
#include <cstddef>
#include <dmt/allocator/adapter.hpp>
#include <dmt/allocator/freelist.hpp>

using namespace dmt::allocator;

TEST_CASE("Freelist allocator", "[allocator::FreeList]") {
  using T = long;
  static constexpr std::size_t SizeOfT = sizeof(T);

  SECTION("DA", "Default allocator can allocate and free") {
    using Allocator = FreeList<>;

    Allocator allocator;
    T* p = reinterpret_cast<T*>(allocator.AllocateUnaligned(SizeOfT));
    REQUIRE(p != nullptr);
    *p = 100;
    allocator.Release(reinterpret_cast<std::byte*>(p));
  }

  SECTION("PSA", "Page-sized allocator (PSA) can fit N objects") {
    static constexpr std::size_t kPageSize = 4096;
    static constexpr std::size_t kNumAllocs = kPageSize / SizeOfT;
    using Allocator = FreeList<SizeT<kPageSize>, GrowT<WhenFull::ReturnNull>,
                               LimitT<ChunksMust::HaveAtLeastSizeBytes>>;

    Allocator allocator;

    std::array<T*, kNumAllocs> allocs = {nullptr};

    for (size_t i = 0; i < kNumAllocs; ++i) {
      T* p = reinterpret_cast<T*>(allocator.AllocateUnaligned(SizeOfT));
      REQUIRE(p != nullptr);
      allocs[i] = p;
    }

    for (size_t i = 0; i < kNumAllocs; ++i) {
      allocator.Release(reinterpret_cast<std::byte*>(allocs[i]));
      allocs[i] = nullptr;
    }
  }
}