#pragma once

#include <result.hpp>

namespace dmt::internal {

// Failures encountered during internal operations.
enum class Failure {
  HeaderIsNullptr,
  InvalidSize,
  InvalidAlignment,
  BlockTooSmall
};

template <class T> using Failable = cpp::result<T, Failure>;

} // namespace dmt::internal
