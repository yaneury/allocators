#pragma once

#include <atomic>
#include <cstdint>
#include <optional>
#include <thread>

#include <template/parameters.hpp>

#include <allocators/common/error.hpp>
#include <allocators/common/parameters.hpp>
#include <allocators/common/trait.hpp>
#include <allocators/internal/platform.hpp>

namespace allocators {

// Coarse-grained allocator that allocates multiples of system page size
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
      std::max({kDefaultMaxSize / internal::GetPageSize() - 1,
                ntp::optional<CountT<0>, Args...>::value});

  Page() = default;

  Result<std::byte*> Allocate(std::size_t count) {
    if (count == 0 || count > kCount)
      return cpp::fail(Error::InvalidInput);

    // TODO: Currently, this allocator doesn't support requesting more than
    //  one page at a time.
    if (count != 1)
      return cpp::fail(Error::OperationNotSupported);

    while (true) {
      auto old_anchor = anchor_.load();
      if (old_anchor.status == Status::Initial) {
        if (auto result = InitializeHeap(); result.has_error())
          return cpp::fail(result.error());

        continue;
      } else if (old_anchor.status == Status::Allocating) {
        std::this_thread::yield();
        continue;
      }

      if (old_anchor.available == 0 || old_anchor.head == kCount)
        return cpp::fail(Error::NoFreeBlock);

      auto new_anchor = old_anchor;
      new_anchor.available = old_anchor.available - 1;
      new_anchor.head = GetHeap()->descriptors[old_anchor.head].next;
      if (anchor_.compare_exchange_weak(old_anchor, new_anchor)) {
        auto& descriptor = GetHeap()->descriptors[old_anchor.head];
        descriptor.occupied = true;
        descriptor.next = 0;
        auto ptr = GetHeap()->super_block.base +
                   old_anchor.head * internal::GetPageSize();
        return reinterpret_cast<std::byte*>(ptr);
      }
    }
  }

  Result<void> Release(std::byte* p) {
    if (p == nullptr || heap_ == std::nullopt)
      return cpp::fail(Error::InvalidInput);

    auto distance =
        reinterpret_cast<std::uintptr_t>(p) -
        reinterpret_cast<std::uintptr_t>(GetHeap()->super_block.base);

    std::size_t index = distance / internal::GetPageSize();
    GetHeap()->descriptors[index].occupied = false;

    while (true) {
      auto old_anchor = anchor_.load();
      auto new_anchor = old_anchor;
      new_anchor.head = index;
      new_anchor.available = old_anchor.available + 1;

      // Eagerly set head here so that if another thread immediately takes
      // this block after the CAS instruction below, the Descriptor entry
      // is in a valid state.
      GetHeap()->descriptors[index].next = old_anchor.head;
      if (anchor_.compare_exchange_weak(old_anchor, new_anchor)) {
        return {};
      }
    }
  }

  [[nodiscard]] constexpr std::size_t GetBlockSize() const {
    return internal::GetPageSize();
  }

private:
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

  enum Status : std::uint64_t {
    Initial = 0,
    Allocating = 1,
    Allocated = 2,
    Failed = 3
  };

  /*
   * Anchor is a bitfield of 64 bits. The bits are outlined below from low bit
   * to high bits.
   *  status: 2 = Status to see if heap has been initialized:
   *    Initial, Allocating, Allocated
   *  head: 18 = Index of current head of LIFO list.
   *  available: 18 = Number of pages available for allocation.
   *    0 if at capacity.
   */
  struct Anchor {
    std::uint64_t status : 2;
    std::uint64_t head : 18;
    std::uint64_t available : 18;
    std::uint64_t reserved : 26;
  };

  Result<void> InitializeHeap() {
    auto old_anchor = anchor_.load();
    if (old_anchor.status != Status::Initial)
      return {};

    auto new_anchor = old_anchor;
    new_anchor.status = Status::Allocating;
    if (!anchor_.compare_exchange_weak(old_anchor, new_anchor))
      return {};

    auto heap_va_range_or =
        internal::FetchPages(sizeof(Heap) / internal::GetPageSize());
    // TODO: Mapping of internal to user-facing error should be more robust.
    if (heap_va_range_or.has_error())
      return cpp::fail(Error::OutOfMemory);

    auto sb_va_range_or = internal::FetchPages(kCount);
    if (sb_va_range_or.has_error())
      return cpp::fail(Error::OutOfMemory);

    auto heap_va_range = heap_va_range_or.value();
    Heap* heap = reinterpret_cast<Heap*>(heap_va_range.base);
    heap->super_block = sb_va_range_or.value();
    for (auto i = 0u; i < kCount; ++i) {
      Descriptor& descriptor = heap->descriptors[i];
      descriptor.occupied = false;
      descriptor.next = i + 1;
    }

    heap_ = heap_va_range;

    new_anchor.available = kCount;
    new_anchor.status = Status::Allocated;
    anchor_.store(new_anchor);
    return {};
  }

  Heap* GetHeap() {
    if (!heap_.has_value())
      return nullptr;

    return reinterpret_cast<Heap*>(heap_->base);
  }

  std::atomic<Anchor> anchor_ = {};
  std::optional<internal::VirtualAddressRange> heap_ = std::nullopt;
};

} // namespace allocators
