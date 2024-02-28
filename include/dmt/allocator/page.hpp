#pragma once

#include <atomic>
#include <cstdint>

#include <template/parameters.hpp>

#include "error.hpp"
#include "internal/common.hpp"
#include "internal/platform.hpp"
#include "parameters.hpp"
#include "trait.hpp"

namespace dmt::allocator {

// Coarse-grained allocator that allocated multiples of system page size
// on request. This is used internally by other allocators in this library
// to fetch memory from the heap. However, it's available for general usage
// in the public API.
//
// This is very limited in practice. Any non-trivial program will quickly exceed
// the maximum number of pages configured. Also consider that certain objects
// can exceed the size of a page. This structure doesn't accommodate those
// requests at all.
template <class... Args> class Page {
public:
  static constexpr std::size_t kDefaultMaxSize = 1 << 30;

  // Max number of pages that this allocator will allow. This is a strict limit.
  // No more than |kCount| pages will be supported.
  // Defaults to 1GB / GetPageSize().
  static constexpr std::size_t kCount =
      std::max({kDefaultMaxSize / internal::GetPageSize(),
                ntp::optional<CountT<0>, Args...>::value});

  Page() = default;

  Result<std::byte*> Allocate(std::size_t count) {
    if (count == 0)
      return cpp::fail(Error::InvalidInput);

    while (true) {
      auto old_anchor = anchor_.load();
      if (!old_anchor.initialized) {
        if (auto result = InitializeHeap(); result.has_error())
          return cpp::fail(result.error());

        continue;
      }

      if (old_anchor.available == 0 || old_anchor.head == kCount)
        return cpp::fail(Error::NoFreeBlock);

      auto new_anchor = old_anchor;
      new_anchor.available = old_anchor.available - 1;
      new_anchor.head = heap_->descriptors[old_anchor.head].next;
      if (anchor_.compare_exchange_weak(old_anchor, new_anchor)) {
        auto& descriptor = heap_->descriptors[old_anchor.head];
        descriptor.occupied = true;
        descriptor.next = 0;
        auto ptr =
            heap_->super_block.base + old_anchor.head * internal::GetPageSize();
        return reinterpret_cast<std::byte*>(ptr);
      }
    }
  }

  Result<void> Release(std::byte* p) {
    if (p == nullptr || heap_ == nullptr)
      return cpp::fail(Error::InvalidInput);

    auto distance = reinterpret_cast<std::uintptr_t>(p) -
                    reinterpret_cast<std::uintptr_t>(heap_->super_block.base);

    std::size_t index = distance / internal::GetPageSize();
    heap_->descriptors[index].occupied = false;

    while (true) {
      auto old_anchor = anchor_.load();
      auto new_anchor = old_anchor;
      new_anchor.head = index;
      new_anchor.available = old_anchor.available + 1;

      // Eagerly set head here so that if another thread immediately takes
      // this block after the CAS instruction below, the Descriptor entry
      // is in a valid state.
      heap_->descriptors[index].next = old_anchor.head;
      if (anchor_.compare_exchange_weak(old_anchor, new_anchor)) {
        return {};
      }
    }
  }

private:
  Result<void> InitializeHeap() { return {}; }

  // A block descriptor is an entry in the linked list of blocks.
  struct Descriptor {
    // Index of next entry in list.
    std::size_t next;

    // Whether this block is currently in use.
    bool occupied;
  };

  struct alignas(internal::GetPageSize()) Heap {
    internal::VirtualAddressRange super_block;
    Descriptor descriptors[kCount];
  };

  /*
   * Anchor is a bitfield of 64 bits. The bits are outlined below from low bit
   * to high bits.
   *  initialized: 1 = Bit to see if heap has been initialized.
   *  head: 18 = Index of current head of LIFO list.
   *  available: 18 = Number of pages available for allocation.
   *    0 if at capacity.
   *  tag: 27 = Used to prevent ABA problem when invoking CAS on anchor.
   */
  struct Anchor {
    std::uint64_t initialized : 1;
    std::uint64_t head : 18;
    std::uint64_t available : 18;
    std::uint64_t reserved : 27;
  };

  std::atomic<Anchor> anchor_ = {};
  Heap* heap_ = nullptr;
};

} // namespace dmt::allocator
