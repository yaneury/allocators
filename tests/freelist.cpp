#include <array>
#include <cstddef>

#include <catch2/catch_all.hpp>
#include <magic_enum.hpp>

#include <dmt/allocator/adapter.hpp>
#include <dmt/allocator/freelist.hpp>

using namespace dmt::allocator;

TEST_CASE("Freelist allocator", "[allocator::FreeList]") {
  using T = long;
  static constexpr std::size_t SizeOfT = sizeof(T);

  SECTION("Default parameters",
          "Can allocate and free using default parameters") {
    using Allocator = FreeList<>;

    Allocator allocator;
    auto p_or = allocator.AllocateUnaligned(SizeOfT);
    REQUIRE(p_or.has_value());

    T* p = reinterpret_cast<T*>(p_or.value());
    REQUIRE(p != nullptr);
    *p = 100;

    REQUIRE(allocator.Release(reinterpret_cast<std::byte*>(p)).has_value());
  }

  SECTION("Page-sized block",
          "Allocator with page-sized block can fully use space") {
    static constexpr std::size_t kChunkSize =
        SizeOfT + dmt::internal::GetBlockHeaderSize();
    static constexpr std::size_t kPageSize = 4096;
    static constexpr std::size_t N = kPageSize / kChunkSize;

    using Allocator = FreeList<SizeT<kPageSize>, GrowT<WhenFull::ReturnNull>,
                               LimitT<BlocksMust::NoMoreThanSizeBytes>>;

    Allocator allocator;

    std::array<T*, N> allocs = {nullptr};

    for (size_t i = 0; i < N; ++i) {
      auto p_or = allocator.AllocateUnaligned(SizeOfT);
      REQUIRE(p_or.has_value());

      T* p = reinterpret_cast<T*>(p_or.value());
      REQUIRE(p != nullptr);
      allocs[i] = p;
    }

    // Should be out of space now.
    REQUIRE(allocator.AllocateUnaligned(1) == cpp::fail(Error::NoFreeBlock));

    for (size_t i = 0; i < N; ++i) {
      REQUIRE(allocator.Release(reinterpret_cast<std::byte*>(allocs[i]))
                  .has_value());
      allocs[i] = nullptr;
    }

    SECTION("Reallocation of freed space",
            "Can reallocate objects using freed space") {
      for (size_t i = 0; i < N; ++i) {
        auto p_or = allocator.AllocateUnaligned(SizeOfT);
        REQUIRE(p_or.has_value());

        T* p = reinterpret_cast<T*>(p_or.value());
        REQUIRE(p != nullptr);
        allocs[i] = p;
      }

      for (size_t i = 0; i < N; ++i) {
        REQUIRE(allocator.Release(reinterpret_cast<std::byte*>(allocs[i]))
                    .has_value());
        allocs[i] = nullptr;
      }
    }

    SECTION("Coalescion of freed chunks",
            "Coalesces free chunks such that page-sized object fits") {
      auto p_or = allocator.AllocateUnaligned(
          kPageSize - dmt::internal::GetBlockHeaderSize());
      REQUIRE(p_or.has_value());

      T* p = reinterpret_cast<T*>(p_or.value());
      REQUIRE(p != nullptr);
      REQUIRE(allocator.Release(reinterpret_cast<std::byte*>(p)).has_value());
    }
  }
}
