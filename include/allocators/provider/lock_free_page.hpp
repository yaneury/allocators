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
#include <allocators/internal/util.hpp>

namespace allocators::provider {

// Provider class that returns page-aligned and page-sized blocks. The page size
// is determined by the platform, 4KB for most scenarios. For the actual page
// size used on particular platform, see |internal::GetPageSize|. This provider
// is thread-safe using lock-free algorithms.
template <class... Args> class LockFreePage;

namespace {
struct Params {
  // Default limit is set to 1GB (1 << 30) of VA range
  // divided by system page size.
  static constexpr std::size_t kDefaultLimit =
      (1 << 30) / internal::GetPageSize() - 1;

  // Max number of pages that Provider will create. This is a strict limit.
  // No more than this number of pages will be supported.
  // Defaults to |kDefaultLimit| / |internal, which is roughly: 1GB /
  // GetPageSize().
  template <std::size_t R>
  struct LimitT : std::integral_constant<std::size_t, R> {};
};
} // namespace

template <class... Args> class LockFreePage : Params {
public:
  LockFreePage() = default;

  ALLOCATORS_NO_COPY_NO_MOVE(LockFreePage);

  Result<std::byte*> Provide(std::size_t count) {
    if (count == 0 || count > kLimit)
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

      if (old_anchor.available == 0 || old_anchor.head == kLimit)
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

  Result<void> Return(std::byte* p) {
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
  static constexpr std::size_t kLimit =
      std::max({kDefaultLimit, ntp::optional<LimitT<0>, Args...>::value});

  // A block descriptor is an entry in the linked list of blocks.
  struct Descriptor {
    // Index of next entry in list.
    std::size_t next;

    // Whether this block is currently in use.
    bool occupied;
  };

  struct alignas(internal::GetPageSize()) Heap {
    internal::VirtualAddressRange super_block;
    Descriptor descriptors[kLimit];
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

    auto sb_va_range_or = internal::FetchPages(kLimit);
    if (sb_va_range_or.has_error())
      return cpp::fail(Error::OutOfMemory);

    auto heap_va_range = heap_va_range_or.value();
    Heap* heap = reinterpret_cast<Heap*>(heap_va_range.base);
    heap->super_block = sb_va_range_or.value();
    for (auto i = 0u; i < kLimit; ++i) {
      Descriptor& descriptor = heap->descriptors[i];
      descriptor.occupied = false;
      descriptor.next = i + 1;
    }

    heap_ = heap_va_range;

    new_anchor.available = kLimit;
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

} // namespace allocators::provider
