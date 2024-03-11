#include "catch2/catch_all.hpp"

#include <vector>

#include <allocators/adapter/adapter.hpp>

using namespace allocators;

using T = long;

TEST_CASE("BumpAdapter allocator works with standard containers",
          "[functional][allocator][BumpAdapter]") {
  SECTION("A fixed-sized allocator that can hold a page worth of objects") {
    static constexpr std::size_t PageSize = 4096;
    using AllocatorUnderTest = adapter::BumpAdapter<T, SizeT<PageSize>>;

    std::vector<T, AllocatorUnderTest> values;
    for (size_t i = 0; i < 100; ++i)
      values.push_back(i);

    SUCCEED(); // Should not panic here.
  }
}
