#include <libdmt/allocator/bump.hpp>
#define CATCH_CONFIG_MAIN
#include <catch2/catch_all.hpp>

using namespace dmt::allocator;

SCENARIO("Bump allocator can allocate objects", "[allocator::Bump]") {
  using T = long;
  static constexpr std::size_t SizeOfT = sizeof(T);

  GIVEN("a fixed-sized allocator that can fit two objects") {
    using Allocator = Bump<T, SizeT<SizeOfT * 2>, GrowT<WhenFull::ReturnNull>>;
    Allocator allocator;

    INFO("AlignedSize: " << Allocator::AlignedSize_);

    T* a = allocator.allocate(SizeOfT);
    WHEN("an object (within size) is allocated") {
      THEN("it is given a valid pointer address") { REQUIRE(a != nullptr); }
    }

    WHEN("another object is allocated") {
      REQUIRE(a != nullptr);
      T* b = allocator.allocate(SizeOfT);
      REQUIRE(b != nullptr);

      THEN("it is set to the address next to the previously allocated one") {
        REQUIRE(a + 1 == b);
      }
    }

    WHEN("deallocate is invoked") {
      REQUIRE(a != nullptr);
      THEN("the allocated objects remain valid") {
        *a = 100;
        allocator.deallocate(a, SizeOfT);
        REQUIRE(*a == 100);
      }
    }

    WHEN("allocate is invoked over capacity") {
      REQUIRE(allocator.allocate(SizeOfT) != nullptr);

      THEN("allocate returns nullptr") {
        REQUIRE(allocator.allocate(SizeOfT) == nullptr);
      }
    }
  }

  GIVEN("a variable-sized allocator that can fit two objects") {
    using Allocator = Bump<T, SizeT<SizeOfT * 2>, GrowT<WhenFull::GrowStorage>>;
    Allocator allocator;

    WHEN("making more allocation than can fit in one chunk") {
      THEN("will grow until OOM") {
        for (size_t i = 0; i < 100; ++i)
          REQUIRE(allocator.allocate(SizeOfT) != nullptr);
      }
    }
  }

  GIVEN("a page-sized allocator that can fit many pages") {
    static constexpr std::size_t PageSize = 4096;
    using Allocator = Bump<T, SizeT<PageSize>, GrowT<WhenFull::GrowStorage>>;
    Allocator allocator;

    WHEN("making an allocation within page") {
      THEN("it allocates") {
        T* a = allocator.allocate(SizeOfT);
        REQUIRE(a != nullptr);
      }
    }

    WHEN("making an allocation greater than page size") {
      THEN("it returns nullptr") {
        REQUIRE(allocator.allocate(PageSize) == nullptr);
      }
    }
  }
}