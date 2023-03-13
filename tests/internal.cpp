#include <algorithm>
#include <catch2/catch_all.hpp>
#include <dmt/internal/chunk.hpp>
#include <dmt/internal/util.hpp>
#include <ranges>
#include <vector>

using namespace dmt::internal;

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

TEST_CASE("AllocateBytes", "[internal/platform]") {
  REQUIRE(AllocateBytes(/*invalid size*/ 0,
                        /*a valid alignment*/ sizeof(void*)) == std::nullopt);
  REQUIRE(AllocateBytes(/*a valid size*/ 100, /*invalid alignment*/ 0) ==
          std::nullopt);
  REQUIRE(AllocateBytes(/*a valid size*/ 100,
                        /*a non-power of two alignment*/ 3) == std::nullopt);
  REQUIRE(AllocateBytes(/*a valid size*/ 100,
                        /*an alignment less than minimum*/ 2) == std::nullopt);
}

TEST_CASE("AllocatePages", "[internal/platform]") {
  REQUIRE(AllocatePages(/*invalid size*/ 0) == std::nullopt);
}

TEST_CASE("ZeroChunk", "[internal/chunk]") {
  // Basic test that passing in a |nullptr| doesn't crash the process.
  // Of course, there's nothing to assert.
  ZeroChunk(nullptr);

  static constexpr std::size_t kBufferSize = 32;
  static constexpr char kTestChar = 'a';

  // Initialize buffer with all 'a' characters to ensure that zero out works
  // as expected.
  char kBuffer[kBufferSize];
  std::fill_n(kBuffer, kBufferSize, 'a');

  ChunkHeader* header = reinterpret_cast<ChunkHeader*>(kBuffer);
  // Add self-pointer to validate that it's not zeroed out.
  header->next = reinterpret_cast<ChunkHeader*>(&kBuffer);
  header->size = kBufferSize;

  for (size_t i = GetChunkHeaderSize(); i < kBufferSize; ++i)
    REQUIRE(kBuffer[i] == 'a');

  ZeroChunk(header);

  REQUIRE(header->next == reinterpret_cast<ChunkHeader*>(&kBuffer));
  REQUIRE(header->size == kBufferSize);
  for (size_t i = GetChunkHeaderSize(); i < kBufferSize; ++i)
    REQUIRE(kBuffer[i] == 0);
}

class TestFreeList {
public:
  static TestFreeList FromChunkSizes(std::vector<std::size_t> chunk_sizes) {
    std::size_t total_size = 0;
    std::ranges::for_each(chunk_sizes, [&total_size](std::size_t& sz) {
      sz += GetChunkHeaderSize();
      total_size += sz;
    });

    auto buffer = std::make_unique<std::byte[]>(total_size);

    std::byte* itr = buffer.get();
    for (std::size_t i = 0; i < chunk_sizes.size(); ++i) {
      auto size = chunk_sizes[i];
      ChunkHeader* h = reinterpret_cast<ChunkHeader*>(itr);
      h->size = size;
      itr = itr + size;
      if (i < chunk_sizes.size() - 1)
        h->next = reinterpret_cast<ChunkHeader*>(itr);
    }

    return TestFreeList(std::move(buffer), std::move(chunk_sizes));
  }

  ChunkHeader* AsHeader() {
    CHECK(buffer_ != nullptr);
    return reinterpret_cast<ChunkHeader*>(buffer_.get());
  }

  ChunkHeader* GetHeader(std::size_t target) {
    CHECK(target < chunk_sizes_.size());

    std::size_t offset =
        std::accumulate(begin(chunk_sizes_), begin(chunk_sizes_) + target, 0);

    return reinterpret_cast<ChunkHeader*>(buffer_.get() + offset);
  }

private:
  TestFreeList() = delete;

  TestFreeList(std::unique_ptr<std::byte[]> buffer,
               std::vector<std::size_t> chunk_sizes)
      : buffer_(std::move(buffer)), chunk_sizes_(std::move(chunk_sizes)) {}

  std::vector<std::size_t> chunk_sizes_;
  std::unique_ptr<std::byte[]> buffer_;
};

[[gnu::const]] inline constexpr std::size_t SizeWithHeader(std::size_t sz) {
  return sz + GetChunkHeaderSize();
}

TEST_CASE("Find Chunk returns std::nullopt on bad input", "[internal/chunk]") {
  auto fn =
      GENERATE(FindChunkByFirstFit, FindChunkByBestFit, FindChunkByWorstFit);

  REQUIRE(fn(nullptr, 5) == std::nullopt);
  REQUIRE(fn(TestFreeList::FromChunkSizes({3, 5, 3}).AsHeader(), 0) ==
          std::nullopt);
}

TEST_CASE("Find Chunk returns std::nullopt if no minimun size found",
          "[internal/chunk]") {
  auto fn =
      GENERATE(FindChunkByFirstFit, FindChunkByBestFit, FindChunkByWorstFit);
  auto free_list = TestFreeList::FromChunkSizes({3, 3, 3});

  auto actual = fn(free_list.AsHeader(), SizeWithHeader(4));

  REQUIRE(actual == std::nullopt);
}

TEST_CASE(
    "FindChunkByFirstFit selects first header even if not optimal choice ",
    "[internal/chunk]") {
  auto free_list = TestFreeList::FromChunkSizes({3, 5, 4});

  auto actual = FindChunkByFirstFit(free_list.AsHeader(), SizeWithHeader(4));

  REQUIRE(actual.has_value());
  REQUIRE(actual.value().prev == free_list.GetHeader(0));
  REQUIRE(actual.value().header == free_list.GetHeader(1));
}

TEST_CASE("FindChunkByBestFit selects header closest to size",
          "[internal/chunk]") {
  auto free_list = TestFreeList::FromChunkSizes({3, 5, 4});

  auto actual = FindChunkByBestFit(free_list.AsHeader(), SizeWithHeader(4));

  INFO("AsHeader: " << free_list.AsHeader());
  INFO("GetHeader(0)" << free_list.GetHeader(0));
  INFO("GetHeader(1)" << free_list.GetHeader(1));
  INFO("GetHeader(2)" << free_list.GetHeader(2));

  REQUIRE(actual.has_value());
  REQUIRE(actual.value().prev == free_list.GetHeader(1));
  REQUIRE(actual.value().header == free_list.GetHeader(2));
}

TEST_CASE("FindChunkByWorstFit selects header furthest away from size",
          "[internal/chunk]") {
  auto free_list = TestFreeList::FromChunkSizes({3, 4, 5});

  auto actual = FindChunkByWorstFit(free_list.AsHeader(), 4);

  REQUIRE(actual.has_value());
  REQUIRE(actual.value().prev == free_list.GetHeader(1));
  REQUIRE(actual.value().header == free_list.GetHeader(2));
}

TEST_CASE("SplitChunk returns error on bad input", "[internal/chunk]") {
  REQUIRE(SplitChunk(nullptr, 5, kMinimumAlignment) ==
          cpp::fail(Failure::HeaderIsNullptr));

  // Allocate singleton free list with large size to ensure no error related
  // to insufficient size is returned.
  auto free_list = TestFreeList::FromChunkSizes({100});

  REQUIRE(SplitChunk(free_list.AsHeader(), 0, kMinimumAlignment) ==
          cpp::fail(Failure::InvalidSize));
  REQUIRE(SplitChunk(free_list.AsHeader(), 1, 0) ==
          cpp::fail(Failure::InvalidAlignment));
}

TEST_CASE("SplitChunk returns error if chunk too small", "[internal/chunk]") {
  std::size_t kAlignment = 8;

  auto free_list = TestFreeList::FromChunkSizes({8});

  REQUIRE(SplitChunk(free_list.AsHeader(), 1 + GetChunkHeaderSize(),
                     kAlignment) == cpp::fail(Failure::ChunkTooSmall));
  REQUIRE(SplitChunk(free_list.AsHeader(), 8 + GetChunkHeaderSize(),
                     kAlignment) == cpp::fail(Failure::ChunkTooSmall));
}

TEST_CASE("SplitChunk splits chunks using alignment", "[internal/chunk]") {
  std::size_t kAlignment = 8;
  std::size_t kChunkSize = kAlignment;

  // Make free list that is able to accomodate two 8-byte chunks after
  // splitting.
  auto free_list =
      TestFreeList::FromChunkSizes({kChunkSize * 2 + GetChunkHeaderSize()});

  ChunkHeader* header = free_list.AsHeader();
  auto actual =
      SplitChunk(header, kChunkSize + GetChunkHeaderSize(), kAlignment);

  REQUIRE(header->size == GetChunkHeaderSize() + kChunkSize);
  REQUIRE(header->next == nullptr);

  auto expected = reinterpret_cast<ChunkHeader*>(
      BytePtr(header) + GetChunkHeaderSize() + kChunkSize);
  REQUIRE(actual == expected);
  REQUIRE((*actual)->size == GetChunkHeaderSize() + kChunkSize);
  REQUIRE((*actual)->next == nullptr);
}

TEST_CASE("CoalesceChunk returns Error on bad input", "[internal/chunk]") {
  REQUIRE(CoalesceChunk(nullptr) == cpp::fail(Failure::HeaderIsNullptr));
}

TEST_CASE("CoalesceChunk merges all free adjacent chunks", "[internal/chunk]") {
  std::size_t kChunkSize = 8;
  auto free_list =
      TestFreeList::FromChunkSizes({kChunkSize, kChunkSize, kChunkSize});

  CHECK(CoalesceChunk(free_list.AsHeader()).has_value());

  ChunkHeader* header = free_list.AsHeader();
  REQUIRE(header->next == nullptr);
  REQUIRE(header->size == 3 * kChunkSize + 3 * GetChunkHeaderSize());
}

TEST_CASE("CoalesceChunk doesn't merge if free chunks are not adjacent",
          "[internal/chunk]") {
  std::size_t kChunkSize = 8;
  auto free_list_a = TestFreeList::FromChunkSizes({kChunkSize});
  auto header_a = free_list_a.AsHeader();
  auto free_list_b = TestFreeList::FromChunkSizes({kChunkSize});
  auto header_b = free_list_b.AsHeader();
  header_a->next = header_b;

  CHECK(CoalesceChunk(header_a).has_value());

  REQUIRE(header_a->next == header_b);
  REQUIRE(header_a->size == kChunkSize + GetChunkHeaderSize());
}
