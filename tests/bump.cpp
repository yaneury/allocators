#include <catch2/catch_all.hpp>
#include <dmt/allocator/adapter.hpp>
#include <dmt/allocator/bump.hpp>

using namespace dmt::allocator;

TEST_CASE("Bump allocator", "[allocator::Bump]") {
  using T = long;
  static constexpr std::size_t SizeOfT = sizeof(T);

  INFO("SizeOfT: " << SizeOfT);

  SECTION("fb2", "a fixed-sized allocator that can fit two objects") {
    using Allocator = Bump<SizeT<SizeOfT * 2>, GrowT<WhenFull::ReturnNull>>;
    Allocator allocator;

    T* a = reinterpret_cast<T*>(allocator.AllocateUnaligned(SizeOfT));
    SECTION("an object (within size) is allocated") { REQUIRE(a != nullptr); }

    SECTION("another object is allocated") {
      REQUIRE(a != nullptr);
      T* b = reinterpret_cast<T*>(allocator.AllocateUnaligned(SizeOfT));
      REQUIRE(b != nullptr);

      SECTION("it is set to the address next to the previously allocated one") {
        auto* addr = reinterpret_cast<std::byte*>(a + 1);
        REQUIRE(addr + dmt::internal::GetChunkHeaderSize() ==
                reinterpret_cast<std::byte*>(b));
      }
    }

    SECTION("deallocate is invoked") {
      REQUIRE(a != nullptr);
      SECTION("the allocated objects remain valid") {
        *a = 100;
        allocator.Release(reinterpret_cast<std::byte*>(a));
        REQUIRE(*a == 100);
      }
    }

    SECTION("allocate is invoked over capacity") {
      REQUIRE(allocator.AllocateUnaligned(SizeOfT) != nullptr);

      SECTION("allocate returns nullptr") {
        REQUIRE(allocator.AllocateUnaligned(SizeOfT) == nullptr);
      }
    }
  }

  SECTION("a variable-sized allocator that can fit two objects") {
    using Allocator = Bump<SizeT<SizeOfT * 2>, GrowT<WhenFull::GrowStorage>>;
    Allocator allocator;

    SECTION("making more allocation than can fit in one chunk") {
      SECTION("will grow until OOM") {
        for (size_t i = 0; i < 100; ++i)
          REQUIRE(allocator.AllocateUnaligned(SizeOfT) != nullptr);
      }
    }
  }

  SECTION("a page-sized allocator that can fit many pages") {
    static constexpr std::size_t PageSize = 4096;
    using Allocator =
        Bump<SizeT<PageSize - dmt::internal::GetChunkHeaderSize()>,
             GrowT<WhenFull::GrowStorage>>;
    Allocator allocator;

    SECTION("making an allocation within page") {
      SECTION("it allocates") {
        T* a = reinterpret_cast<T*>(allocator.AllocateUnaligned(SizeOfT));
        REQUIRE(a != nullptr);
      }
    }

    SECTION("making an allocation greater than page size") {
      SECTION("it returns nullptr") {
        REQUIRE(allocator.AllocateUnaligned(PageSize) == nullptr);
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