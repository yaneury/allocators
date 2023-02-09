#include <libdmt/allocator/bump.hpp>
#define CATCH_CONFIG_MAIN
#include <catch2/catch_all.hpp>

SCENARIO("Bump allocator can allocate objects", "[allocator::Bump]") {
  GIVEN("an integer allocator with a size of sizeof(int) * 2") {
    using T = long;
    static constexpr std::size_t SizeOfT = sizeof(T);

    using Allocator =
        dmt::allocator::Bump<T, /*StorageSize=*/SizeOfT * 2>;
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
}