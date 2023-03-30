#pragma once

#include <array>

#include <template/parameters.hpp>

#include "error.hpp"
#include "parameters.hpp"
#include "trait.hpp"

namespace dmt::allocator {

template <class... Args> class Fixed {
public:
  // Size of the memory block. This *must* match the size of the buffer passed
  // to the object constructor.
  static constexpr std::size_t kSize =
      ntp::optional<SizeT<DMT_ALLOCATOR_SIZE>, Args...>::value;

  using Buffer = std::array<std::byte, kSize>;

  Fixed() = default;

  Fixed(Buffer& buffer) : buffer_(buffer) {}

  Fixed(Buffer&& buffer) : buffer_(std::move(buffer)) {}

  Result<std::byte*> Allocate(Layout layout) {
    if (layout.size == 0 || layout.alignment == 0)
      return cpp::fail(Error::InvalidInput);

    std::size_t space_remaining = kSize - end_;
    if (layout.size > space_remaining)
      return cpp::fail(Error::ReachedMemoryLimit);

    std::byte* ptr = &buffer_[end_];
    end_ += layout.size;
    return ptr;
  }

  Result<std::byte*> Allocate(std::size_t size) {
    // This allocator does not enforce alignment.
    return Allocate(Layout(size, /*alignment=*/1));
  }

  Result<void> Release(std::byte* ptr) {
    if (!ptr)
      return cpp::fail(Error::InvalidInput);

    return {};
  }

private:
  Buffer& buffer_;
  std::size_t end_ = 0;
};

} // namespace dmt::allocator
