#include <algorithm>
#include <catch2/catch_all.hpp>
#include <dmt/internal/chunk.hpp>
#include <dmt/internal/util.hpp>
#include <vector>

using namespace dmt::internal;

TEST_CASE("IsPowerOfTwo is computed", "[internal::IsPowerOfTwo]") {
  size_t kMaxExp = 16;
  std::vector<size_t> powers_of_two = {};
  for (size_t i = 0; i < kMaxExp; ++i) {
    powers_of_two.push_back(pow(static_cast<size_t>(2), i));
  }

  for (size_t i = 0; i < powers_of_two.back(); ++i) {
    bool expected = std::find(powers_of_two.cbegin(), powers_of_two.cend(),
                              i) != powers_of_two.cend();
    REQUIRE(IsPowerOfTwo(i) == expected);
  }
}

TEST_CASE("AlignUp", "[internal::AlignUp]") {
  REQUIRE(AlignUp(4095, 4096) == 4096);
  REQUIRE(AlignUp(0, 8) == 0);
  REQUIRE(AlignUp(8, 0) == 0);
  REQUIRE(AlignUp(4, 4) == 4);
  REQUIRE(AlignUp(11, 8) == 16);
}

TEST_CASE("AllocateBytes", "[internal::AllocateBytes]") {
  REQUIRE(AllocateBytes(/*invalid size */ 0,
                        /*a valid alignment=*/sizeof(void*)) == std::nullopt);
  REQUIRE(AllocateBytes(/*a valid size*/ 100, /*invalid alignment*/ 0) ==
          std::nullopt);
  REQUIRE(AllocateBytes(/* a valid size */ 100,
                        /*a non-power of two alignment*/ 3) == std::nullopt);
  REQUIRE(AllocateBytes(/* a valid size */ 100,
                        /*an alignment less than minimum*/ 2) == std::nullopt);
}