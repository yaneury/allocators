#pragma once

#include <atomic>
#include <memory>
#include <mutex>

#include <template/parameters.hpp>

#include "block.hpp"
#include "error.hpp"
#include "internal/block.hpp"
#include "internal/util.hpp"
#include "trait.hpp"

namespace dmt::allocator {

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
template <class... Args> class Bump : public Block<Args...> {
public:
  // We have to explicitly provide the parent class in contexts with a
  // nondepedent name. For more information, see:
  // https://stackoverflow.com/questions/75595977/access-protected-members-of-base-class-when-using-template-parameter.
  using Parent = Block<Args...>;

  using Allocator = typename Parent::Allocator;

  using Options = typename Parent::Options;

  explicit Bump(Options options = Parent::kDefaultOptions)
      : Parent(Allocator(), std::move(options)) {}

  explicit Bump(Allocator& allocator, Options options = Parent::kDefaultOptions)
      : Parent(allocator, std::move(options)) {}

  Bump(Allocator&& allocator, Options options)
      : Parent(std::move(allocator), std::move(options)) {}

  // TODO: Don't ignore this error.
  ~Bump() { (void)Reset(); }

  Result<std::byte*> Allocate(Layout layout) noexcept {
    if (!IsValid(layout))
      return cpp::fail(Error::InvalidInput);

    std::size_t request_size = internal::AlignUp(layout.size, layout.alignment);
    DINFO("Request Size: " << request_size);

    if (request_size > Parent::GetAlignedSize())
      return cpp::fail(Error::SizeRequestTooLarge);

    // The loop here is a little deceiving. The intention here is not to
    // loop endlessly. Instead, the loop aids the subsequent atomic operations.
    // In practice, no more than one iteration will be needed.
    while (true) {
      BlockDescriptor old_active = active.load();
      if (!old_active.initialized) {
        if (auto result = AllocateNewBlock(); result.has_error())
          return cpp::fail(result.error());

        continue;
      }

      std::size_t headroom = Parent::GetAlignedSize() - old_active.size;
      if (headroom < request_size) {
        if (!Parent::kGrowWhenFull)
          return cpp::fail(Error::ReachedMemoryLimit);

        if (auto result = AllocateNewBlock(); result.has_error())
          return cpp::fail(result.error());

        continue;
      }

      BlockDescriptor new_active = old_active;
      new_active.offset = old_active.offset + request_size;
      if (active.compare_exchange_weak(old_active, new_active))
        return block_table[old_active.index] + old_active.offset;
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
    auto old_active = active.load();
    if (!old_active.initialized)
      return {};

    for (auto i = 0u; i <= old_active.index; i++) {
      if (auto result = Parent::allocator_.Release(block_table[i]);
          result.has_error())
        return cpp::fail(result.error());

      block_table[i] = nullptr;
    }

    active.store(BlockDescriptor());

    return {};
  }

private:
  // This only allows ~1,000 entries which isn't a lot. Initially, this was set
  // to 20 bits, but that blew the static data space, causing immediate
  // segfaults.
  // TODO: Figure out a way to improve block_table size without ballooning
  // virtual
  //  address space. Perhaps, we can model something like the page table
  //  structures where multiple tables are used to determine the final address.
  static constexpr unsigned kTotalEntryInBits = 10;

  struct BlockDescriptor {
    // Whether the block was initialized.
    std::uint64_t initialized : 1;

    // Index in |block_table|.
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
    auto old_active = active.load();
    auto new_active = old_active;
    new_active.offset = 0;
    if (old_active.initialized)
      new_active.index = old_active.index + 1;
    // We always set this to help with the init case where |active| is
    // 0.
    new_active.initialized = 1;

    auto new_block_or = Parent::allocator_.Allocate(Parent::GetAlignedSize());
    if (new_block_or.has_error())
      return cpp::fail(Error::OutOfMemory);

    if (active.compare_exchange_weak(old_active, new_active)) {
      block_table[new_active.index] = new_block_or.value();
    } else if (auto result = Parent::allocator_.Release(new_block_or.value());
               result.has_error()) {
      return cpp::fail(result.error());
    }

    return {};
  }

  std::atomic<BlockDescriptor> active = BlockDescriptor();
  std::array<std::byte*, 1 << kTotalEntryInBits> block_table = {0};
};

} // namespace dmt::allocator
