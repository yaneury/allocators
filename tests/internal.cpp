#include <algorithm>
#include <ranges>
#include <vector>

#include <catch2/catch_all.hpp>
#include <catch2/catch_test_macros.hpp>

#include <dmt/allocator/internal/block.hpp>
#include <dmt/allocator/internal/util.hpp>

using namespace dmt::allocator::internal;

TEST_CASE("IsPowerOfTwo", "[internal/util]") {
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

TEST_CASE("IsValidRequest", "[internal/util]") {
  REQUIRE(IsValidRequest(/*size of zero*/ 0, kMinimumAlignment) == false);
  REQUIRE(IsValidRequest(1, /*alignment of zero*/ 0) == false);
  REQUIRE(IsValidRequest(/*size of zero*/ 0, /*and alignment of zero*/ 0) ==
          false);
  REQUIRE(IsValidRequest(1, /*alignment less than minimum*/ 4) == false);
  REQUIRE(IsValidRequest(1, /*alignment not power of two*/ 13) == false);
  REQUIRE(IsValidRequest(1, /*alignment not power of two*/ 13) == false);
}

TEST_CASE("AlignUp", "[internal/util]") {
  REQUIRE(AlignUp(4095, 4096) == 4096);
  REQUIRE(AlignUp(0, 8) == 0);
  REQUIRE(AlignUp(8, 0) == 0);
  REQUIRE(AlignUp(4, 4) == 4);
  REQUIRE(AlignUp(11, 8) == 16);
}

TEST_CASE("AllocatePages", "[internal/platform]") {
  REQUIRE(AllocatePages(/*invalid size*/ 0) == std::nullopt);
}

TEST_CASE("ZeroBlock", "[internal/block]") {
  // Basic test that passing in a |nullptr| doesn't crash the process.
  // Of course, there's nothing to assert.
  ZeroBlock(nullptr);

  static constexpr std::size_t kBufferSize = 32;
  static constexpr char kTestChar = 'a';

  // Initialize buffer with all 'a' characters to ensure that zero out works
  // as expected.
  char kBuffer[kBufferSize];
  std::fill_n(kBuffer, kBufferSize, 'a');

  BlockHeader* header = reinterpret_cast<BlockHeader*>(kBuffer);
  // Add self-pointer to validate that it's not zeroed out.
  header->next = reinterpret_cast<BlockHeader*>(&kBuffer);
  header->size = kBufferSize;

  for (size_t i = GetBlockHeaderSize(); i < kBufferSize; ++i)
    REQUIRE(kBuffer[i] == 'a');

  ZeroBlock(header);

  REQUIRE(header->next == reinterpret_cast<BlockHeader*>(&kBuffer));
  REQUIRE(header->size == kBufferSize);
  for (size_t i = GetBlockHeaderSize(); i < kBufferSize; ++i)
    REQUIRE(kBuffer[i] == 0);
}

class TestFreeList {
public:
  static TestFreeList FromBlockSizes(std::vector<std::size_t> block_sizes) {
    std::size_t total_size = 0;
    std::ranges::for_each(block_sizes, [&total_size](std::size_t& sz) {
      sz += GetBlockHeaderSize();
      total_size += sz;
    });

    auto buffer = std::make_unique<std::byte[]>(total_size);

    std::byte* itr = buffer.get();
    for (std::size_t i = 0; i < block_sizes.size(); ++i) {
      auto size = block_sizes[i];
      BlockHeader* h = reinterpret_cast<BlockHeader*>(itr);
      h->size = size;
      itr = itr + size;
      if (i < block_sizes.size() - 1)
        h->next = reinterpret_cast<BlockHeader*>(itr);
    }

    return TestFreeList(std::move(buffer), std::move(block_sizes));
  }

  BlockHeader* AsHeader() {
    CHECK(buffer_ != nullptr);
    return reinterpret_cast<BlockHeader*>(buffer_.get());
  }

  BlockHeader* GetHeader(std::size_t target) {
    CHECK(target < block_sizes_.size());

    std::size_t offset =
        std::accumulate(begin(block_sizes_), begin(block_sizes_) + target, 0);

    return reinterpret_cast<BlockHeader*>(buffer_.get() + offset);
  }

private:
  TestFreeList() = delete;

  TestFreeList(std::unique_ptr<std::byte[]> buffer,
               std::vector<std::size_t> block_sizes)
      : buffer_(std::move(buffer)), block_sizes_(std::move(block_sizes)) {}

  std::vector<std::size_t> block_sizes_;
  std::unique_ptr<std::byte[]> buffer_;
};

[[gnu::const]] inline constexpr std::size_t SizeWithHeader(std::size_t sz) {
  return sz + GetBlockHeaderSize();
}

TEST_CASE("Find Block returns std::nullopt on bad input", "[internal/block]") {
  auto fn =
      GENERATE(FindBlockByFirstFit, FindBlockByBestFit, FindBlockByWorstFit);

  REQUIRE(fn(nullptr, 5) == std::nullopt);
  REQUIRE(fn(TestFreeList::FromBlockSizes({3, 5, 3}).AsHeader(), 0) ==
          std::nullopt);
}

TEST_CASE("Find Block returns std::nullopt if no minimun size found",
          "[internal/block]") {
  auto fn =
      GENERATE(FindBlockByFirstFit, FindBlockByBestFit, FindBlockByWorstFit);
  auto free_list = TestFreeList::FromBlockSizes({3, 3, 3});

  auto actual = fn(free_list.AsHeader(), SizeWithHeader(4));

  REQUIRE(actual == std::nullopt);
}

TEST_CASE(
    "FindBlockByFirstFit selects first header even if not optimal choice ",
    "[internal/block]") {
  auto free_list = TestFreeList::FromBlockSizes({3, 5, 4});

  auto actual = FindBlockByFirstFit(free_list.AsHeader(), SizeWithHeader(4));

  REQUIRE(actual.has_value());
  REQUIRE(actual.value().prev == free_list.GetHeader(0));
  REQUIRE(actual.value().header == free_list.GetHeader(1));
}

TEST_CASE("FindBlockByBestFit selects header closest to size",
          "[internal/block]") {
  auto free_list = TestFreeList::FromBlockSizes({3, 5, 4});

  auto actual = FindBlockByBestFit(free_list.AsHeader(), SizeWithHeader(4));

  INFO("AsHeader: " << free_list.AsHeader());
  INFO("GetHeader(0)" << free_list.GetHeader(0));
  INFO("GetHeader(1)" << free_list.GetHeader(1));
  INFO("GetHeader(2)" << free_list.GetHeader(2));

  REQUIRE(actual.has_value());
  REQUIRE(actual.value().prev == free_list.GetHeader(1));
  REQUIRE(actual.value().header == free_list.GetHeader(2));
}

TEST_CASE("FindBlockByWorstFit selects header furthest away from size",
          "[internal/block]") {
  auto free_list = TestFreeList::FromBlockSizes({3, 4, 5});

  auto actual = FindBlockByWorstFit(free_list.AsHeader(), 4);

  REQUIRE(actual.has_value());
  REQUIRE(actual.value().prev == free_list.GetHeader(1));
  REQUIRE(actual.value().header == free_list.GetHeader(2));
}

TEST_CASE("FindPriorBlock returns Failable on bad input", "[internal/block]") {
  auto free_list = TestFreeList::FromBlockSizes({3, 4, 5});

  REQUIRE(FindPriorBlock(nullptr, free_list.AsHeader()) ==
          cpp::fail(Failure::HeaderIsNullptr));
  REQUIRE(FindPriorBlock(free_list.AsHeader(), nullptr) ==
          cpp::fail(Failure::HeaderIsNullptr));
}

TEST_CASE("FindPriorBlock returns nullptr if |block| past or equal to |head|",
          "[internal/block]") {
  auto free_list = TestFreeList::FromBlockSizes({3, 4, 5});

  REQUIRE(FindPriorBlock(free_list.GetHeader(1), free_list.GetHeader(0)) ==
          nullptr);
  REQUIRE(FindPriorBlock(free_list.GetHeader(0), free_list.GetHeader(0)) ==
          nullptr);
}

TEST_CASE("FindPriorBlock returns block prior to |block|", "[internal/block]") {
  auto free_list = TestFreeList::FromBlockSizes({3, 4, 5});

  REQUIRE(FindPriorBlock(free_list.GetHeader(0), free_list.GetHeader(2)) ==
          free_list.GetHeader(1));
  REQUIRE(FindPriorBlock(free_list.GetHeader(1), free_list.GetHeader(2)) ==
          free_list.GetHeader(1));
}

TEST_CASE("SplitBlock returns error on bad input", "[internal/block]") {
  REQUIRE(SplitBlock(nullptr, 5, kMinimumAlignment) ==
          cpp::fail(Failure::HeaderIsNullptr));

  // Allocate singleton free list with large size to ensure no error related
  // to insufficient size is returned.
  auto free_list = TestFreeList::FromBlockSizes({100});

  REQUIRE(SplitBlock(free_list.AsHeader(), 0, kMinimumAlignment) ==
          cpp::fail(Failure::InvalidSize));
  REQUIRE(SplitBlock(free_list.AsHeader(), 1, 0) ==
          cpp::fail(Failure::InvalidAlignment));
}

TEST_CASE("SplitBlock returns nullptr if block too small", "[internal/block]") {
  std::size_t kAlignment = 8;

  auto free_list = TestFreeList::FromBlockSizes({8});

  REQUIRE(SplitBlock(free_list.AsHeader(), 1 + GetBlockHeaderSize(),
                     kAlignment) == nullptr);
  REQUIRE(SplitBlock(free_list.AsHeader(), 8 + GetBlockHeaderSize(),
                     kAlignment) == nullptr);
}

TEST_CASE("SplitBlock splits blocks using alignment", "[internal/block]") {
  std::size_t kAlignment = 8;
  std::size_t kBlockSize = kAlignment;

  // Make free list that is able to accomodate two 8-byte blocks after
  // splitting.
  auto free_list =
      TestFreeList::FromBlockSizes({kBlockSize * 2 + GetBlockHeaderSize()});

  BlockHeader* header = free_list.AsHeader();
  auto actual =
      SplitBlock(header, kBlockSize + GetBlockHeaderSize(), kAlignment);

  REQUIRE(header->size == GetBlockHeaderSize() + kBlockSize);
  REQUIRE(header->next == actual);

  auto expected = reinterpret_cast<BlockHeader*>(
      BytePtr(header) + GetBlockHeaderSize() + kBlockSize);
  REQUIRE(actual == expected);
  REQUIRE((*actual)->size == GetBlockHeaderSize() + kBlockSize);
  REQUIRE((*actual)->next == nullptr);
}

TEST_CASE("CoalesceBlock returns Error on bad input", "[internal/block]") {
  REQUIRE(CoalesceBlock(nullptr) == cpp::fail(Failure::HeaderIsNullptr));
}

TEST_CASE("CoalesceBlock merges all free adjacent blocks", "[internal/block]") {
  std::size_t kBlockSize = 8;
  auto free_list =
      TestFreeList::FromBlockSizes({kBlockSize, kBlockSize, kBlockSize});

  CHECK(CoalesceBlock(free_list.AsHeader()).has_value());

  BlockHeader* header = free_list.AsHeader();
  REQUIRE(header->next == nullptr);
  REQUIRE(header->size == 3 * kBlockSize + 3 * GetBlockHeaderSize());
}

TEST_CASE("CoalesceBlock doesn't merge if free blocks are not adjacent",
          "[internal/block]") {
  std::size_t kBlockSize = 8;
  auto free_list_a = TestFreeList::FromBlockSizes({kBlockSize});
  auto header_a = free_list_a.AsHeader();
  auto free_list_b = TestFreeList::FromBlockSizes({kBlockSize});
  auto header_b = free_list_b.AsHeader();
  header_a->next = header_b;

  CHECK(CoalesceBlock(header_a).has_value());

  REQUIRE(header_a->next == header_b);
  REQUIRE(header_a->size == kBlockSize + GetBlockHeaderSize());
}
