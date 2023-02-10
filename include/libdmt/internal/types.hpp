#pragma once

#include <cstddef>

namespace dmt {
namespace internal {

template <typename Default, typename... Args> struct GetValueT;

template <typename Default, typename T2, typename... Args>
struct GetValueT<Default, T2, Args...> {
  template <typename D2, typename T22, typename Enable = void>
  struct impl : std::integral_constant<decltype(Default::value),
                                       GetValueT<Default, Args...>::value> {};

  template <typename Default2, typename T22>
  struct impl<Default2, T22,
              std::enable_if_t<std::is_same<typename Default2::Id_,
                                            typename T22::Id_>::value>>
      : std::integral_constant<decltype(Default::value), T22::value> {};

  static constexpr const auto value = impl<Default, T2>::value;
};

template <typename Default>
struct GetValueT<Default>
    : std::integral_constant<decltype(Default::value), Default::value> {};

} // namespace internal
} // namespace dmt