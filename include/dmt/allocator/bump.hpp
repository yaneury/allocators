#pragma once

#include <latch>

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

  Bump(Options options = Parent::kDefaultOptions)
      : Parent(Allocator(), std::move(options)) {}

  Bump(Allocator& allocator, Options options = Parent::kDefaultOptions)
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

    if (request_size > GetMaxRequestSize())
      return cpp::fail(Error::SizeRequestTooLarge);

    while (true) {
      if (tail_.load() == nullptr) {
        if (auto result = AllocateNewBlock(); result.has_error())
          return cpp::fail(result.error());

        continue;
      }

      std::byte* current_offset = offset_.load();
      if (!WithinRange(current_offset, request_size)) {
        if (!Parent::kGrowWhenFull)
          return cpp::fail(Error::ReachedMemoryLimit);

        if (auto result = AllocateNewBlock(); result.has_error())
          return cpp::fail(result.error());

        continue;
      }

      std::byte* new_offset = current_offset + request_size;
      if (offset_.compare_exchange_weak(current_offset, new_offset)) {
        return current_offset;
      }
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
    std::lock_guard<std::mutex> lock(blocks_mutex_);
    offset_ = 0;
    if (blocks_)
      (void)Parent::ReleaseAllBlocks(blocks_); // TODO: Add error handling.
    blocks_ = nullptr;
    return {};
  }

private:
  // Max size allowed per request when accounting for aligned size and block
  // header.
  constexpr std::size_t GetMaxRequestSize() {
    return Parent::GetAlignedSize() - internal::GetBlockHeaderSize();
  }

  Result<void> AllocateNewBlock() {
    bool leader = am_block_allocation_leader_.load();
    if (!leader && am_block_allocation_leader_.compare_exchange_weak(leader, true)) {
      // This thread is the allocation leader
      auto block_or = Parent::AllocateNewBlock();
      if (block_or.has_error()) {
        block_barrier_.count_down();
        last_ = cpp::fail(Error::OutOfMemory);
        return cpp::fail(Error::OutOfMemory);
      }

      auto block = block_or.value();
      last_ = {};
      if (tail_) {
        block->next = tail_;
      }

      tail_ = block;
      block_barrier_.count_down();
    } else {
      block_barrier_.wait();
      if (last_.has_error())
        return last_;
    }
    return {};
  }

  bool WithinRange(std::byte* offset, std::size_t size) {
    return (offset + size) < (offset + Parent::GetAlignedSize());
  }

  std::atomic<internal::BlockHeader*> tail_ = nullptr;
  std::atomic<std::byte*> offset_ = nullptr;

  std::latch block_barrier_ = std::latch(1);
  std::atomic<bool> am_block_allocation_leader_ = false;
  Result<void> last_ = {};
};

} // namespace dmt::allocator
