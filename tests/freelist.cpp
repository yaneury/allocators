#include <array>
#include <catch2/catch_all.hpp>
#include <cstddef>
#include <dmt/allocator/adapter.hpp>
#include <dmt/allocator/freelist.hpp>
#include <magic_enum.hpp>

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
    static constexpr std::size_t N = 2;
    static constexpr std::size_t kChunkSize =
        SizeOfT + dmt::internal::GetBlockHeaderSize();
    static constexpr std::size_t kBlockSize = kChunkSize * N;

    using Allocator = FreeList<SizeT<kBlockSize>, GrowT<WhenFull::ReturnNull>,
                               LimitT<BlocksMust::NoMoreThanSizeBytes>>;

    Allocator allocator;

    // INFO("AlignedSize: " << Allocator::kAlignedSize_);

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

    SECTION("PSAR", "Can reallocate objects using freed space") {
      for (size_t i = 0; i < N; ++i) {
        auto p_or = allocator.AllocateUnaligned(SizeOfT);
        INFO("Error: " << magic_enum::enum_name(p_or.error()));
        REQUIRE(p_or.has_value());

        T* p = reinterpret_cast<T*>(p_or.value());
        INFO("I: " << i);
        REQUIRE(p != nullptr);
        allocs[i] = p;
      }

      for (size_t i = 0; i < N; ++i) {
        REQUIRE(allocator.Release(reinterpret_cast<std::byte*>(allocs[i]))
                    .has_value());
        allocs[i] = nullptr;
      }
    }

    SECTION("PSAC", "Coalesces free block such that block-sized object fits") {
      auto p_or = allocator.AllocateUnaligned(
          kBlockSize - dmt::internal::GetBlockHeaderSize());
      REQUIRE(p_or.has_value());

      T* p = reinterpret_cast<T*>(p_or.value());
      REQUIRE(p != nullptr);
      REQUIRE(allocator.Release(reinterpret_cast<std::byte*>(p)).has_value());
    }
  }
}
