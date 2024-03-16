#include "catch2/catch_all.hpp"

#include <cstddef>
#include <cstring>
#include <stack>
#include <vector>

#include <allocators/internal/block_map.hpp>

#include "../util.hpp"

using namespace allocators::internal;

constexpr std::size_t kBlockSize = 4096;

std::byte* GetBlockZeroedOut() {
  // This doesn't return a dangling pointer since it's declared static, right?
  static std::byte kBlock[kBlockSize] = {std::byte(0)};
  bzero(&kBlock[0], kBlockSize);
  return &kBlock[0];
}

using TypedBlockMap = BlockMap<kBlockSize>;

TEST_CASE("BlockMap is set to empty by default",
          "[functional][internal][BlockMap]") {
  TypedBlockMap* block_as_map = AsBlockMapPtr<kBlockSize>(GetBlockZeroedOut());

  REQUIRE(block_as_map->IsEmpty());
}

TEST_CASE("BlockMap stores up to |GetCapacity| number of entries",
          "[functional][internal][BlockMap]") {
  TypedBlockMap* block_as_map = AsBlockMapPtr<kBlockSize>(GetBlockZeroedOut());

  for (auto i = 0; i < block_as_map->GetCapacity(); ++i) {
    REQUIRE(block_as_map->Insert(VirtualAddressRange()));
    REQUIRE(block_as_map->GetSize() == i + 1);
  }

  REQUIRE(block_as_map->IsFull());

  for (auto i = 0; i < block_as_map->GetCapacity(); ++i)
    REQUIRE(block_as_map->Take(/*address=*/0).has_value());

  REQUIRE(block_as_map->IsEmpty());
}

TEST_CASE("BlockMap returns nullptr for |GetNext| by default",
          "[functional][internal][BlockMap]") {
  TypedBlockMap* block_as_map = AsBlockMapPtr<kBlockSize>(GetBlockZeroedOut());

  REQUIRE(block_as_map->GetNext() == nullptr);
}

TEST_CASE("BlockMap returns pointer to next block after using |SetNext|",
          "[functional][internal][BlockMap]") {
  std::byte* block_as_bytes = GetBlockZeroedOut();
  TypedBlockMap* block_as_map = AsBlockMapPtr<kBlockSize>(GetBlockZeroedOut());

  block_as_map->SetNext(AsBytePtr(block_as_map));

  REQUIRE(block_as_map->GetNext() == block_as_map);
}

TEST_CASE("BlockMap contains value after Insert",
          "[functional][internal][BlockMap]") {
  TypedBlockMap* block_as_map = AsBlockMapPtr<kBlockSize>(GetBlockZeroedOut());

  VirtualAddressRange va_range = {.address = 100, .count = 10};
  REQUIRE(block_as_map->Insert(va_range));
  REQUIRE(block_as_map->GetSize() == 1);

  auto actual_or = block_as_map->Take(va_range.address);
  REQUIRE(actual_or.has_value());
  REQUIRE(actual_or.value() == va_range);
}

TEST_CASE("BlockMap returns std::nullopt if given unknown key",
          "[functional][internal][BlockMap]") {
  TypedBlockMap* block_as_map = AsBlockMapPtr<kBlockSize>(GetBlockZeroedOut());

  VirtualAddressRange va_range = {.address = 100, .count = 10};

  auto actual_or = block_as_map->Take(va_range.address);
  REQUIRE(!actual_or.has_value());
}
