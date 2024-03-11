#pragma once

#include <template/parameters.hpp>

#include <cppalloc/error.hpp>
#include <cppalloc/parameters.hpp>

namespace cppalloc {

// A wrapper over statically-initialized array that conforms to
// the BlockAllocator interface. Unlike other allocators, memory
// is not fetched from the heap, but rather defined at compile time
// as static data.
template <class... Args> class Static {
public:
  // Size of the memory block.
  static constexpr std::size_t kSize =
      ntp::optional<SizeT<CPPALLOC_ALLOCATOR_SIZE>, Args...>::value;

  Static() = default;

  /*
  Static(Static&) = delete;
  Static& operator=(Static&) = delete;

  Static(Static&&) = delete;
  Static& operator=(Static&&) = delete;
  */

  Result<std::byte*> Allocate(std::size_t count) {
    if (count != 1)
      return cpp::fail(Error::InvalidInput);

    return AsPtr();
  }

  Result<void> Release(std::byte* ptr) {
    if (ptr != AsPtr())
      return cpp::fail(Error::InvalidInput);

    return {};
  }

  constexpr std::size_t GetBlockSize() const { return kSize; };

private:
  std::byte* AsPtr() { return &block_[0]; }

  std::byte block_[kSize] = {std::byte(0)};
};

} // namespace cppalloc
