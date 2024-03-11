#pragma once

#include <atomic>

#include <template/parameters.hpp>

#include <cppalloc/block/page.hpp>
#include <cppalloc/error.hpp>
#include <cppalloc/internal/util.hpp>
#include <cppalloc/parameters.hpp>
#include <cppalloc/trait.hpp>

namespace cppalloc {

// A simple Bump allocator. This allocator creates a big block of bytes on
// first allocation, hereafter "block", that fits a large number of objects.
// Each allocation moves a pointer upward, tracking the location of the most
// recent allocation. When an allocation request is made that is greater than
// the size remaining in the current block, this allocator may optionally
// request for more memory, creating a linked list of blocks. Per-object
// allocation, i.e. the standard `free` call, is not supported. Instead, this
// allocator only supports freeing an entire block(s).
//
// This allocation is very fast at performing allocations, but of course limited
// in its utility. It is most appropriate for so-called "phase-based"
// allocations, where objects are created in a group in a phase, and all freed
// at the same time.
//
// For more information about this form of memory allocation, visit:
// https://www.gingerbill.org/article/2019/02/08/memory-allocation-strategies-002.
template <class... Args> class Bump {
public:
  // Allocator used to request memory from OS.
  // Defaults to unconfigured Page allocator.
  using Allocator = typename ntp::type<BlockAllocatorT<Page<>>, Args...>::value;

  // Policy employed when block has no more space for pending request.
  // If |GrowStorage| is provided, then a new block will be requested;
  // if |ReturnNull| is provided, then nullptr is returned on the allocation
  // request. This does not mean that it's impossible to request more memory
  // though. It only means that the block has no more space for the requested
  // size. If a request with a smaller size comes along, it may be possible that
  // the block has sufficient storage for it.
  static constexpr bool kGrowWhenFull =
      ntp::optional<GrowT<CPPALLOC_ALLOCATOR_GROW>, Args...>::value ==
      WhenFull::GrowStorage;

  struct Options {
    std::size_t size;
    bool grow_when_full;
  };

  static constexpr Options kDefaultOptions = {
      .grow_when_full = kGrowWhenFull,
  };

  explicit Bump(Options options = kDefaultOptions)
      : allocator_(Allocator()), options_(std::move(options)) {}

  explicit Bump(Allocator& allocator, Options options = kDefaultOptions)
      : allocator_(allocator), options_(std::move(options)) {}

  Bump(Allocator&& allocator, Options options)
      : allocator_(std::move(allocator)), options_(std::move(options)) {}

  // Allocator is neither copy-able nor move-able.
  Bump(Bump&) = delete;
  Bump(Bump&&) = delete;
  Bump& operator=(Bump&) = delete;
  Bump& operator=(Bump&&) = delete;

  // TODO: Don't ignore this error.
  ~Bump() { (void)Reset(); }

  Result<std::byte*> Allocate(Layout layout) noexcept {
    if (!IsValid(layout))
      return cpp::fail(Error::InvalidInput);

    std::size_t request_size = internal::AlignUp(layout.size, layout.alignment);

    if (request_size > allocator_.GetBlockSize())
      return cpp::fail(Error::SizeRequestTooLarge);

    // The loop here is a little deceiving. The intention here is not to
    // loop endlessly. Instead, the loop aids the subsequent atomic operations.
    // In practice, no more than one iteration will be needed.
    while (true) {
      BlockDescriptor old_active = active_.load();
      if (!old_active.initialized) {
        if (auto result = AllocateNewBlock(); result.has_error())
          return cpp::fail(result.error());

        continue;
      }

      std::size_t headroom = allocator_.GetBlockSize() - old_active.size;
      if (headroom < request_size) {
        if (!kGrowWhenFull)
          return cpp::fail(Error::ReachedMemoryLimit);

        if (auto result = AllocateNewBlock(); result.has_error())
          return cpp::fail(result.error());

        continue;
      }

      BlockDescriptor new_active = old_active;
      new_active.offset = old_active.offset + request_size;
      if (active_.compare_exchange_weak(old_active, new_active))
        return block_table_[old_active.index] + old_active.offset;
    }
  }

  Result<std::byte*> Allocate(std::size_t size) noexcept {
    return Allocate(Layout(size, internal::kMinimumAlignment));
  }

  Result<void> Release(std::byte* ptr) {
    // The bump allocator does not support per-object deallocation.
    return cpp::fail(Error::OperationNotSupported);
  }

  Result<void> Reset() {
    auto old_active = active_.load();
    if (!old_active.initialized)
      return {};

    for (auto i = 0u; i <= old_active.index; i++) {
      if (auto result = allocator_.Release(block_table_[i]); result.has_error())
        return cpp::fail(result.error());

      block_table_[i] = nullptr;
    }

    active_.store(BlockDescriptor());

    return {};
  }

private:
  // This only allows ~1,000 descriptors which isn't a lot. Initially, this was
  // set to 20 bits, but that blew the static data space, causing immediate
  // segfaults.
  // TODO: Figure out a way to improve block_table_ size without ballooning
  //  virtual address space. Perhaps, we can model something like the page table
  //  structures where multiple tables are used to determine the final address.
  static constexpr unsigned kTotalEntryInBits = 10;

  struct BlockDescriptor {
    // Whether the block was status.
    std::uint64_t initialized : 1;

    // Index in |block_table_|.
    std::uint64_t index : kTotalEntryInBits;

    // Multiples of 4KB. Supports block size of ~262MB.
    std::uint64_t size : 16;

    // Current offset within block. Next allocation will return pointer
    // based off this position.
    // We need 25 bits below because size can scale up to ~262MB: log2(4KB * 16)
    // = ~25
    std::uint64_t offset : 25;

    std::uint64_t _unused : 2;
  };

  Result<void> AllocateNewBlock() {
    auto old_active = active_.load();
    auto new_active = old_active;
    new_active.offset = 0;
    if (old_active.initialized)
      new_active.index = old_active.index + 1;
    // We always set this to help with the init case where |active_| is
    // 0.
    new_active.initialized = 1;

    auto new_block_or = allocator_.Allocate(1);
    if (new_block_or.has_error())
      return cpp::fail(Error::OutOfMemory);

    if (active_.compare_exchange_weak(old_active, new_active)) {
      block_table_[new_active.index] = new_block_or.value();
    } else if (auto result = allocator_.Release(new_block_or.value());
               result.has_error()) {
      return cpp::fail(result.error());
    }

    return {};
  }

  // Backing allocator to used to acquire and release blocks.
  Allocator allocator_;

  // Override options for allocator.
  Options options_;

  // Tracking anchor for currently active_ block.
  std::atomic<BlockDescriptor> active_ = BlockDescriptor();

  // Table of all allocated blocks.
  std::array<std::byte*, 1 << kTotalEntryInBits> block_table_ = {0};
};

} // namespace cppalloc
