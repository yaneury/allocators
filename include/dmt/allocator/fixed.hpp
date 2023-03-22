#pragma once

#include <array>

#include <template/parameters.hpp>

#include "error.hpp"
#include "parameters.hpp"
#include "trait.hpp"

namespace dmt::allocator {

template <class... Args> class Fixed {
public:
  // Size of the blocks. This allocator doesn't support variable-sized blocks.
  // All blocks allocated are of the same size. N.b. that the size here will
  // *not* be the size of memory ultimately requested for blocks. This is so
  // because supplemental memory is needed for block headers and to ensure
  // alignment as specified with |kAlignment|.
  //
  // This field is optional. If not provided, will default to the common page
  // size, 4096.
  static constexpr std::size_t kSize =
      ntp::optional<SizeT<4096>, Args...>::value;

  Result<std::byte*> Allocate(Layout layout) {
    if (layout.size == 0)
      return cpp::fail(Error::InvaldInput);

    std::size_t space_remaining = kSize - end_;
    if (layout.size > space_remaining)
      return cpp::fail(Error::ReachedMemoryLimit);

    std::byte* ptr = buffer_[end_];
    end_ += layout.size;
    return ptr;
  }

  Result<std::byte*> Allocate(std::size_t size) {
    // This allocator does not enforce alignment.
    return Allocate(Layout(size, /*alignment=*/1));
  }

  Result<void> Release(std::byte* ptr) { return {}; }

private:
  std::array<std::byte, kSize> buffer_;
  std::size_t end_ = 0;
};

} // namespace dmt::allocator
