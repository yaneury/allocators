#include <libdmt/allocator/bump.hpp>
#define CATCH_CONFIG_MAIN
#include <catch2/catch_all.hpp>

SCENARIO("Bump allocator can allocate objects", "[allocator::Bump]") {
  GIVEN("an integer allocator with a size of sizeof(int) * 2") {
    using Allocator =
        dmt::allocator::Bump</*T=*/int, /*StorageSize=*/sizeof(int) * 2>;
    Allocator allocator;

    int* a = allocator.allocate(sizeof(int));
    WHEN("an object (within size) is allocated") {
      THEN("it is given a valid pointer address") { REQUIRE(a != nullptr); }
    }

    WHEN("another object is allocated") {
      REQUIRE(a != nullptr);
      int* b = allocator.allocate(sizeof(int));
      REQUIRE(b != nullptr);

      THEN("it is set to the address next to the previously allocated one") {
        REQUIRE(a + 1 == b);
      }
    }

    WHEN("deallocate is invoked") {
      REQUIRE(a != nullptr);
      THEN("the allocated objects remain valid") {
        *a = 100;
        allocator.deallocate(a, sizeof(int));
        REQUIRE(*a == 100);
      }
    }

    WHEN("allocate is invoked over capacity") {
      REQUIRE(allocator.allocate(sizeof(int)) != nullptr);

      REQUIRE(allocator.allocate(sizeof(int)) == nullptr);
    }
  }
}