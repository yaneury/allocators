#pragma once

#include <array>
#include <cstddef>
#include <cstdlib>
#include <functional>
#include <random>
#include <utility>

#include "internal/fixed_map.hpp"

namespace dmt::handle {

class Compactor {
public:
  // TODO: Use shared Result type from "allocator/error.hpp"
  std::byte* Allocate() { return nullptr; }

  // TODO: Use shared Result type from "allocator/error.hpp"
  void Release(std::byte* ptr) {}

  void Compact(std::function<void(std::byte*, std::byte*)> relocate) {}
};

using Id = std::size_t;

static constexpr Id kUnsetHandle = 0;

static constexpr Id kMinHandle = 1;
// Arbitrary handle limit set for prototyping purposes.
static constexpr Id kHandleLimit = 1 << 8;

class Provider {
public:
  Provider() : mt_(rd_()), dist_(kMinHandle, kHandleLimit) {
    for (auto& id_ptr : handle_table_) {
      id_ptr.first = kUnsetHandle;
      id_ptr.second = nullptr;
    }
  }

  Id Request(std::size_t size) {
    void* p = malloc(size);
    if (!p)
      return 0;

    Id id = GetRandomId();
    handle_table_[0] = std::make_pair(id, reinterpret_cast<std::byte*>(p));
    return id;
  }

  void Release(Id handle) {}

  void ForceRelocation() {
    handle_table_[1] = handle_table_[0];
    handle_table_[0].first = kUnsetHandle;
    handle_table_[0].second = nullptr;
  }

  std::byte* GetCurrentAddress(Id handle) {
    for (auto [id, ptr] : handle_table_)
      if (id == handle)
        return ptr;

    return nullptr;
  }

private:
  Id GetRandomId() { return dist_(mt_); }

  // TODO: Create compile-time map structure, backed by std::array perhaps.
  std::array<std::pair<Id, std::byte*>, kHandleLimit> handle_table_;

  internal::FixedMap<Id, std::byte*, kHandleLimit> handles_;

  std::random_device rd_;
  std::mt19937 mt_;
  std::uniform_int_distribution<Id> dist_;
};

} // namespace dmt::handle
