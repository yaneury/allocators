#include <catch2/catch_all.hpp>
#include <dmt/allocator/adapter.hpp>
#include <dmt/allocator/freelist.hpp>

using namespace dmt::allocator;

TEST_CASE("Freelist allocator", "[allocator::FreeList]") {
  using Allocator = FreeList<>;
}