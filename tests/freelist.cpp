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
    auto p_or = allocator.AllocateUnaligned(SizeOfT);
    REQUIRE(p_or.has_value());

    T* p = reinterpret_cast<T*>(p_or.value());
    REQUIRE(p != nullptr);
    *p = 100;
    REQUIRE(allocator.Release(reinterpret_cast<std::byte*>(p)).has_value());
  }

  SECTION("PSA", "Page-sized allocator (PSA) can fit N objects") {
    static constexpr std::size_t kPageSize = 4096;
    static constexpr std::size_t kNumAllocs =
        kPageSize / (SizeOfT + dmt::internal::GetChunkHeaderSize());
    using Allocator = FreeList<SizeT<kPageSize>, GrowT<WhenFull::ReturnNull>,
                               LimitT<ChunksMust::HaveAtLeastSizeBytes>>;

    Allocator allocator;

    std::array<T*, kNumAllocs> allocs = {nullptr};

    for (size_t i = 0; i < kNumAllocs; ++i) {
      auto p_or = allocator.AllocateUnaligned(SizeOfT);
      REQUIRE(p_or.has_value());

      T* p = reinterpret_cast<T*>(p_or.value());
      REQUIRE(p != nullptr);
      allocs[i] = p;
    }

    // Should be out of space now.
    REQUIRE(allocator.AllocateUnaligned(1) ==
            cpp::fail(Error::ReachedMemoryLimit));

    for (size_t i = 0; i < kNumAllocs; ++i) {
      REQUIRE(allocator.Release(reinterpret_cast<std::byte*>(allocs[i]))
                  .has_value());
      allocs[i] = nullptr;
    }

    SECTION("PSAR", "Can reallocate objects using freed space") {
      for (size_t i = 0; i < kNumAllocs; ++i) {
        auto p_or = allocator.AllocateUnaligned(SizeOfT);
        REQUIRE(p_or.has_value());

        T* p = reinterpret_cast<T*>(p_or.value());
        INFO("I: " << i);
        REQUIRE(p != nullptr);
        allocs[i] = p;
      }

      for (size_t i = 0; i < kNumAllocs; ++i) {
        REQUIRE(allocator.Release(reinterpret_cast<std::byte*>(allocs[i]))
                    .has_value());
        allocs[i] = nullptr;
      }
    }

    SECTION("PSAC", "Coalesces free block such that page-sized object fits") {
      auto p_or = allocator.AllocateUnaligned(
          kPageSize - dmt::internal::GetChunkHeaderSize());
      REQUIRE(p_or.has_value());

      T* p = reinterpret_cast<T*>(p_or.value());
      REQUIRE(p != nullptr);
      REQUIRE(allocator.Release(reinterpret_cast<std::byte*>(p)).has_value());
    }
  }
}