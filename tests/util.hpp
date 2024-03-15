#pragma once

#include <algorithm>
#include <cstddef>
#include <memory>
#include <vector>

#include "catch2/catch_test_macros.hpp"
#include "magic_enum.hpp"

#include "allocators/common/error.hpp"
#include "allocators/internal/block.hpp"

using namespace allocators;
using namespace allocators::internal;

template <class T> inline T* FromBytePtr(std::byte* p) {
  return reinterpret_cast<T*>(p);
}

template <class T> inline std::byte* ToBytePtr(T* p) {
  return reinterpret_cast<std::byte*>(p);
}

template <class T> inline T GetValueOrFail(Result<T> result) {
  if (!result.has_value())
    UNSCOPED_INFO(
        "Result failed with: " << magic_enum::enum_name(result.error()));

  REQUIRE(result.has_value());
  return result.value();
}

template <class T> inline T* GetPtrOrFail(Result<std::byte*> result) {
  if (!result.has_value())
    UNSCOPED_INFO(
        "Result failed with: " << magic_enum::enum_name(result.error()));

  REQUIRE(result.has_value());
  return reinterpret_cast<T*>((result.value()));
}

inline constexpr std::size_t SizeWithHeader(std::size_t sz) {
  return sz + GetBlockHeaderSize();
}

template <class T> inline T GetRandomNumber(T low, T high) {
  std::random_device random_device;
  std::mt19937 engine(random_device());
  std::uniform_int_distribution<> distribution(low, high);
  return distribution(engine);
}

class TestFreeList {
public:
  static TestFreeList FromBlockSizes(std::vector<std::size_t> block_sizes) {
    std::size_t total_size = 0;
    for (auto& bz : block_sizes) {
      bz += GetBlockHeaderSize();
      total_size += bz;
    }

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
