#include "catch2/catch_all.hpp"
#include <cstdint>

#include "dmt/allocator/fixed.hpp"
#include <iterator>

#include "../util.hpp"

using namespace dmt::allocator;

using T = long;
static constexpr std::size_t SizeOfT = sizeof(T);
static constexpr std::size_t N = 2;
static constexpr std::size_t kBlockSize = SizeOfT * N;

TEST_CASE("Fixed allocator that can fit N objects", "[allocator][fixed]") {
  using Allocator = Fixed<SizeT<kBlockSize>>;

  Allocator allocator;
  auto buffer = *allocator.GetBuffer();

  for (std::size_t i = 0; i < kBlockSize; ++i)
    buffer[i] = std::byte(0);

  std::array<T*, N> allocs;
  for (std::size_t i = 0; i < N; ++i)
    allocs[i] = GetPtrOrFail<T>(allocator.Allocate(SizeOfT));

  SECTION("All objects are neighbors to each other") {
    for (std::size_t i = 0; i < N - 1; ++i)
      REQUIRE(allocs[i] + 1 == allocs[i + 1]);
  }

  SECTION("Values are stored in contiguous memory, with no headers") {
    for (std::size_t i = 0; i < N; ++i) {
      T value = (i + 1) * 1'000'000;
      *(allocs[i]) = value;

      T* actual = reinterpret_cast<T*>(&buffer[(i * SizeOfT)]);
      REQUIRE(*actual == value);
    }
  }

  SECTION("Can not allocate more objects when at capacity") {
    REQUIRE(allocator.Allocate(SizeOfT) ==
            cpp::fail(Error::ReachedMemoryLimit));
  }

  SECTION("Release is effectively a no-op") {
    *(allocs.front()) = 100;
    REQUIRE(allocator.Release(ToBytePtr(allocs.front())).has_value());
    REQUIRE(*(allocs.front()) == 100);
  }
}
