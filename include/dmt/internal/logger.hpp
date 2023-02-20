#pragma once

#include <concepts> // TODO: Guard this against a C++20 check
#include <string>
#include <type_traits>

namespace dmt::internal {

template <class T>
concept Logger = requires(T logger, std::string s) {
  { logger.debug(s) } -> std::same_as<void>;
  { logger.info(s) } -> std::same_as<void>;
  { logger.warning(s) } -> std::same_as<void>;
  { logger.error(s) } -> std::same_as<void>;
};

} // namespace dmt::internal