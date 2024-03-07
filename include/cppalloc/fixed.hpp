#pragma once

#include <array>

#include <template/parameters.hpp>

#include "error.hpp"
#include "parameters.hpp"
#include "trait.hpp"

namespace cppalloc {

// An allocator that "allocates" bytes on a fixed buffer (i.e. array of bytes).
// The use case for this is to allow pre-allocating chunks of memory and using
// the standard allocation functions on the reserved memory. This is useful
// if the max number of bytes to be used is known a priori, and the number of
// requests to the heap want to be minimized. Of course note that what is
// avoided in heap allocation is paid for in a larger stack.
template <class... Args> class Fixed {
public:
  // Size of the memory block.
  static constexpr std::size_t kSize =
      ntp::optional<SizeT<CPPALLOC_ALLOCATOR_SIZE>, Args...>::value;

  using Buffer = std::array<std::byte, kSize>;

  Fixed() = default;

  std::string s_;

  void SetDebug(std::string s) { s_ = s; }

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

  [[nodiscard]] constexpr std::size_t GetBlockSize() const { return kSize; }

  Buffer* GetBuffer() { return &buffer_; }

private:
  Buffer buffer_;
  std::size_t end_ = 0;
};

} // namespace cppalloc
