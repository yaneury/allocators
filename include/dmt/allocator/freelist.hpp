#pragma once

#include <cstddef>
#include <dmt/allocator/chunk.hpp>
#include <dmt/allocator/trait.hpp>
#include <dmt/internal/util.hpp>

namespace dmt::allocator {

template <class... Args> class FreeList : public Chunk<Args...> {
public:
  FreeList() = default;
  ~FreeList() = default;

  std::byte* AllocateUnaligned(std::size_t size) {
    return Allocate(
        Layout{.size = size, .alignment = internal::kMinimumAlignment});
  }

  std::byte* Allocate(Layout layout) noexcept {}

  void Release(std::byte*) {}

private:
  // We have to explicitly provide the parent class in contexts with a
  // nondepedent name. For more information, see:
  // https://stackoverflow.com/questions/75595977/access-protected-members-of-base-class-when-using-template-parameter.
  using Parent = Chunk<Args...>;
};

} // namespace dmt::allocator