#include <libdmt/allocator/bump.hpp>
#define CATCH_CONFIG_MAIN
#include <catch2/catch_all.hpp>

TEST_CASE("allocator::Bump") {
  dmt::allocator::Bump<int> allocator;

  int *a = allocator.allocate(sizeof(int));
  REQUIRE(a != nullptr);

  *a = 100;
  allocator.deallocate(a, sizeof(int));
  REQUIRE(*a == 100);

  int *b = allocator.allocate(sizeof(int));
  REQUIRE(b != nullptr);
  REQUIRE(*a == 100);
  REQUIRE(a + sizeof(int) == b);
}