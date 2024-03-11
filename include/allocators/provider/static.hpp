#pragma once

#include <template/parameters.hpp>

#include <allocators/common/error.hpp>

namespace allocators::provider {

// Provider class that reserves memory static data instead of
// fetching it from the heap. This is useful if a user wants to
// leverage the various Strategy algorithms on statically-allocated
// memory over that fetched from the heap.
// The |Size| parameter specifies how much memory to reserve.
// Note, the size can be too large, and its up to the user of
// this class to restrict the upper bound size.
template <std::size_t Size, class... Args> class Static {
public:
  Static() = default;

  ALLOCATORS_NO_COPY_NO_MOVE(Static);

  Result<std::byte*> Provide(std::size_t count) {
    if (count != 1)
      return cpp::fail(Error::InvalidInput);

    return AsPtr();
  }

  Result<void> Return(std::byte* ptr) {
    if (ptr != AsPtr())
      return cpp::fail(Error::InvalidInput);

    return {};
  }

  constexpr std::size_t GetBlockSize() const { return Size; };

private:
  std::byte* AsPtr() { return &block_[0]; }

  std::byte block_[Size] = {std::byte(0)};
};

} // namespace allocators::provider
