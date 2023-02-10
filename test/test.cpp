#include <libdmt/allocator/bump.hpp>
#define CATCH_CONFIG_MAIN
#include <catch2/catch_all.hpp>

using namespace dmt::allocator;

SCENARIO("Bump allocator can allocate objects", "[allocator::Bump]") {
  using T = long;
  static constexpr std::size_t SizeOfT = sizeof(T);

  /*
  GIVEN("an allocator that can fit two objects") {
    using Allocator = Bump<T, StorageSizeT<SizeOfT * 2>>;
    Allocator allocator;

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

      REQUIRE(allocator.allocate(SizeOfT) == nullptr);
    }
  }
  */

  GIVEN("an allocator with counter-based freeing") {
    using Allocator =
        Bump<T, StorageSizeT<SizeOfT>, FreeT<Free::WhenCounterZero>>;
    Allocator allocator;

    T* a = allocator.allocate(SizeOfT);
    REQUIRE(a != nullptr);

    WHEN("all outstanding objects are freed") {
      allocator.deallocate(a, SizeOfT);

      THEN("it should be possible to allocate new objects") {
        REQUIRE(allocator.allocate(SizeOfT) != nullptr);
      }
    }
  }
}