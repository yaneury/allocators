// Adapter classes for |std::allocator_traits|. Consult each class'
// documentation for parameters and description of the allocator.
// The documentation for |std::allocator_traits| can be found at
// https://en.cppreference.com/w/cpp/memory/allocator_traits.

#pragma once

#include <cstddef>

#include <allocators/common/trait.hpp>
#include <allocators/provider/lock_free_page.hpp>
#include <allocators/strategy/lock_free_bump.hpp>

namespace allocators::adapter {

template <class T, class... Args>
class BumpAdapter
    : public strategy::LockFreeBump<provider::LockFreePage<>, Args...> {
public:
  // Require alias for std::allocator_traits to infer other types, e.g.
  // using pointer = value_type*.
  using value_type = T;

  using Parent = strategy::LockFreeBump<Args...>;

  T* allocate(std::size_t n) noexcept {
    Result<std::byte*> ptr_or = Parent::Allocate(n);
    if (ptr_or.has_error())
      return nullptr;
    return reinterpret_cast<T*>(ptr_or.value());
  }

  void deallocate(T*, std::size_t) noexcept {}
};

template <class T, class U>
bool operator==(const BumpAdapter<T>&, const BumpAdapter<U>&) {
  return true;
}

template <class T, class U>
bool operator!=(const BumpAdapter<T>&, const BumpAdapter<U>&) {
  return false;
}
} // namespace allocators::adapter
