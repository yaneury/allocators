#pragma once

#include <concepts>

namespace dmt::allocator::internal {

template <class T>
requires std::integral<T>
struct BitField {
  const T width;
  const T offset;

  constexpr T GetMax() const { return (T(1) << width) - T(1); }

  constexpr T GetMask() const { return GetMax() << offset; }

  constexpr T Get(T bitset) const { return (bitset >> offset) & GetMax(); }

  constexpr T Replace(T bitset, T value) const {
    value <<= offset;
    value &= GetMax();
    return (bitset & ~GetMask()) | value;
  }
};

} // namespace dmt::allocator::internal