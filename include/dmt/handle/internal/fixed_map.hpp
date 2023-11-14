
#pragma once

#include <array>
#include <cstddef>
#include <cstdlib>
#include <functional>
#include <optional>
#include <random>
#include <utility>

namespace dmt::handle::internal {

// Ref: https://en.cppreference.com/w/cpp/experimental/make_array
// Basic map-type with `constexpr` support in constructor
template <class Key, class Value, std::size_t N> class FixedMap {
public:
  constexpr FixedMap() = default;

  template <class... Pairs> constexpr FixedMap(Pairs... kvs) {
    inner_map_ = {kvs...};
  }

  std::optional<Value> Insert(Key key, Value value) {
    for (auto& [k, v] : inner_map_) {
      if (k == key) {
        std::optional<Value> result = v;
        v = value;
        return result;
      }
    }

    return std::nullopt;
  }

  std::optional<Value&> Get(Key key) {
    for (auto& [k, v] : inner_map_)
      if (k == key)
        return v;

    return std::nullopt;
  }

  bool Contains(Key key) const {
    for (auto& [k, _] : inner_map_)
      if (k == key)
        return true;

    return false;
  }

  bool AtCapacity() const { return true; }

private:
  std::array<std::pair<Key, Value>, N> inner_map_;
  std::size_t elements_ = 0;
};

} // namespace dmt::handle::internal
