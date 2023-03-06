#include <algorithm>
#include <catch2/catch_all.hpp>
#include <dmt/internal/chunk.hpp>
#include <dmt/internal/util.hpp>
#include <vector>
#include <queue>

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

std::unique_ptr<std::byte[]>
CreateFreeList(std::vector<std::size_t> chunk_sizes) {
  std::transform(chunk_sizes.begin(), chunk_sizes.end(), chunk_sizes.begin(), [](auto sz) {
    return sz + GetChunkHeaderSize();
  });

  std::size_t total_size = 0;
  for (auto size: chunk_sizes)
    total_size += size;

  auto buffer = std::make_unique<std::byte[]>(total_size);

  std::byte* itr = buffer.get();
  for (std::size_t i = 0; i < chunk_sizes.size(); ++i) {
    auto size = chunk_sizes[i];
    ChunkHeader* h = reinterpret_cast<ChunkHeader*>(itr);
    h->size = size;
    itr = itr+size;
    if (i < chunk_sizes.size() - 1)
      h->next = reinterpret_cast<ChunkHeader*>(itr);
  }

 return std::move(buffer);
}

TEST_CASE("FindChunkByFirstFit", "[internal/chunk]") {
  auto buffer = CreateFreeList({3, 5, 3});
  auto head = reinterpret_cast<ChunkHeader*>(buffer.get());
  auto actual = FindChunkByFirstFit(head, 5);

  REQUIRE(actual.has_value());

  REQUIRE(actual.value().prev == head);
  REQUIRE(actual.value().header == reinterpret_cast<ChunkHeader*>(buffer.get() + 3 + GetChunkHeaderSize()));
}

TEST_CASE("FindChunkByBestFit", "[internal/chunk]") {}

TEST_CASE("FindChunkByWorstFit", "[internal/chunk]") {}

TEST_CASE("SplitChunk", "[internal/chunk]") {}

TEST_CASE("CoalesceChunk", "[internal/chunk]") {}