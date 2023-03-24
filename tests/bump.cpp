#include <catch2/catch_all.hpp>

#include <dmt/allocator/adapter.hpp>
#include <dmt/allocator/bump.hpp>

#include "test_util.hpp"

using namespace dmt::allocator;

TEST_CASE("Bump allocator", "[allocator::Bump]") {
  using T = long;
  static constexpr std::size_t SizeOfT = sizeof(T);

  INFO("SizeOfT: " << SizeOfT);

  SECTION("fb2", "a fixed-sized allocator that can fit two objects") {
    using Allocator = Bump<SizeT<SizeOfT * 2>, GrowT<WhenFull::ReturnNull>,
                           LimitT<BlocksMust::HaveAtLeastSizeBytes>>;
    Allocator allocator;

    T* a = GetPtrOrFail<T>(allocator.Allocate(SizeOfT));
    SECTION("an object (within size) is allocated") { REQUIRE(a != nullptr); }

    SECTION("another object is allocated") {
      REQUIRE(a != nullptr);
      T* b = GetPtrOrFail<T>(allocator.Allocate(SizeOfT));
      REQUIRE(b != nullptr);

      SECTION("it is set to the address next to the previously allocated one") {
        REQUIRE(a + 1 == b);
      }
    }

    SECTION("deallocate is invoked") {
      REQUIRE(a != nullptr);
      SECTION("the allocated objects remain valid") {
        *a = 100;
        REQUIRE(allocator.Release(reinterpret_cast<std::byte*>(a)).has_value());
        REQUIRE(*a == 100);
      }
    }
    SECTION("allocate is invoked over capacity") {
      REQUIRE(allocator.Allocate(SizeOfT) != nullptr);

      SECTION("allocate returns nullptr") {
        REQUIRE(allocator.Allocate(SizeOfT) == nullptr);
      }
    }
  }

  SECTION("a variable-sized allocator that can fit two objects") {
    using Allocator = Bump<SizeT<SizeOfT * 2>, GrowT<WhenFull::GrowStorage>>;
    Allocator allocator;

    SECTION("making more allocation than can fit in one block") {
      SECTION("will grow until OOM") {
        for (size_t i = 0; i < 100; ++i)
          REQUIRE(allocator.Allocate(SizeOfT) != nullptr);
      }
    }
  }

  SECTION("pb", "a page-sized, page-aligned allocator with growth storage") {
    static constexpr std::size_t PageSize = 4096;
    using Allocator =
        Bump<SizeT<PageSize - internal::GetBlockHeaderSize()>,
             AlignmentT<PageSize>, LimitT<BlocksMust::HaveAtLeastSizeBytes>,
             GrowT<WhenFull::GrowStorage>>;
    Allocator allocator;

    SECTION("making an allocation within page") {
      SECTION("it allocates") {
        T* a =
            reinterpret_cast<T*>(GetValueOrFail(allocator.Allocate(SizeOfT)));
        REQUIRE(a != nullptr);
      }
    }

    // When accounting for header, page size should be larger than size of
    // block.
    SECTION("making an allocation greater than block size") {
      SECTION("it returns nullptr") {
        REQUIRE(allocator.Allocate(PageSize) == nullptr);
      }
    }
  }
}

TEST_CASE("BumpAdapter allocator", "[allocator::BumpAdapter]") {
  using T = long;

  SECTION("a fixed-sized allocator that can hold a page worth of objects") {
    static constexpr std::size_t PageSize = 4096;
    using Allocator = BumpAdapter<T, SizeT<PageSize>>;

    std::vector<T, Allocator> values;
    for (size_t i = 0; i < 100; ++i)
      values.push_back(i);

    REQUIRE(true); // Should not panic here.
  }
}
