#pragma once

#include <cstddef>

namespace dmt::handle {

using Id = std::size_t;

class Provider {
public:
  Provider() = default;

  Id Request(std::size_t size) { return 0; }

  void Release(Id handle) {}

  std::byte* GetCurrentAddress(Id handle) { return nullptr; }
};

} // namespace dmt::handle
