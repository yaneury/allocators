#include "catch2/catch_all.hpp"

#include <cstddef>
#include <cstring>
#include <stack>
#include <vector>

#include <allocators/internal/block_array.hpp>

using namespace allocators::internal;

constexpr std::size_t kBlockSize = 4096;

std::byte* GetBlockZeroedOut() {
  // This doesn't return a dangling pointer since it's declared static, right?
  static std::byte kBlock[kBlockSize] = {std::byte(0)};
  bzero(&kBlock[0], kBlockSize);
  return &kBlock[0];
}

using TypedBlockArray = BlockArray<kBlockSize, std::uint64_t>;

TEST_CASE("BlockArray is set to empty by default",
          "[functional][internal][BlockArray]") {
  TypedBlockArray* block_as_array =
      AsBlockArrayPtr<kBlockSize, std::uint64_t>(GetBlockZeroedOut());

  REQUIRE(block_as_array->IsEmpty());
}

TEST_CASE("BlockArray stores up to |GetCapacity| number of entries",
          "[functional][internal][BlockArray]") {
  TypedBlockArray* block_as_array =
      AsBlockArrayPtr<kBlockSize, std::uint64_t>(GetBlockZeroedOut());

  for (auto i = 0; i < block_as_array->GetCapacity(); ++i)
    block_as_array->PushBackUnchecked(std::uint64_t());

  REQUIRE(block_as_array->GetSize() == block_as_array->GetCapacity());
}

TEST_CASE("BlockArray stores elements in sequential order",
          "[functional][internal][BlockArray]") {
  TypedBlockArray* block_as_array =
      AsBlockArrayPtr<kBlockSize, std::uint64_t>(GetBlockZeroedOut());

  std::stack<std::uint64_t> values;

  for (auto i = 0; i < block_as_array->GetCapacity(); ++i) {
    auto v = i + 1;
    values.push(v);
    block_as_array->PushBackUnchecked(v);
  }

  while (values.size()) {
    REQUIRE(values.top() == block_as_array->PopBackUnchecked());
    values.pop();
  }

  REQUIRE(block_as_array->IsEmpty());
}

TEST_CASE("BlockArray swaps with last element when removing in the middle",
          "[functional][internal][BlockArray]") {
  static constexpr std::size_t kSize = 5;
  TypedBlockArray* block_as_array =
      AsBlockArrayPtr<kBlockSize, std::uint64_t>(GetBlockZeroedOut());

  std::vector<std::uint64_t> values;
  for (auto i = 0; i < kSize; ++i) {
    auto v = i + 1;
    values.push_back(v);
    block_as_array->PushBackUnchecked(v);
  }

  // The layout of the array should be [1, 2, 3, 4, 5]
  // So if we pop 3 and then 4 it should be [1, 2, 5]
  // 3 swaps with 5, and 4 should be the end of array so disregarded it.
  block_as_array->Remove(3);
  block_as_array->Remove(4);

  REQUIRE(block_as_array->GetSize() == 3);
  REQUIRE(block_as_array->PopBackUnchecked() == 5);
  REQUIRE(block_as_array->PopBackUnchecked() == 2);
  REQUIRE(block_as_array->PopBackUnchecked() == 1);
  REQUIRE(block_as_array->IsEmpty());
}

TEST_CASE("BlockArray returns nullptr for |GetNext| by default",
          "[functional][internal][BlockArray]") {
  TypedBlockArray* block_as_array =
      AsBlockArrayPtr<kBlockSize, std::uint64_t>(GetBlockZeroedOut());

  REQUIRE(block_as_array->GetNext() == nullptr);
}

TEST_CASE("BlockArray returns pointer to next block after using |SetNext|",
          "[functional][internal][BlockArray]") {
  std::byte* block_as_bytes = GetBlockZeroedOut();
  TypedBlockArray* block_as_array =
      AsBlockArrayPtr<kBlockSize, std::uint64_t>(block_as_bytes);

  block_as_array->SetNext(reinterpret_cast<std::uintptr_t>(block_as_array));

  REQUIRE(block_as_array->GetNext() == block_as_array);
}
