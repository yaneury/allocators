#pragma once

#include <string>

#include "magic_enum.hpp"

#include <result.hpp>

namespace cppalloc::internal {

// Failures encountered during internal operations.
enum class Failure {
  HeaderIsNullptr,
  InvalidSize,
  InvalidAlignment,
  BlockTooSmall,
  AllocationFailed,
  ReleaseFailed,
};

inline std::string_view ToString(Failure failure) {
  return magic_enum::enum_name(failure);
}

template <class T> using Failable = cpp::result<T, Failure>;

} // namespace cppalloc::internal
