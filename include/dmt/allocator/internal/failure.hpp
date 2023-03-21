#pragma once

#include <result.hpp>

namespace dmt::allocator::internal {

// Failures encountered during internal operations.
enum class Failure {
  HeaderIsNullptr,
  InvalidSize,
  InvalidAlignment,
  BlockTooSmall,
  AllocationFailed,
};

template <class T> using Failable = cpp::result<T, Failure>;

} // namespace dmt::allocator::internal
