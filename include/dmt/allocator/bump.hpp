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
      : Parent(Allocator(), std::move(options)), tail_(GetSentinel()),
        offset_(Parent::GetAlignedSize()) {}

  explicit Bump(Allocator& allocator, Options options = Parent::kDefaultOptions)
      : Parent(allocator, std::move(options)), tail_(GetSentinel()),
        offset_(Parent::GetAlignedSize()) {}

  Bump(Allocator&& allocator, Options options)
      : Parent(std::move(allocator), std::move(options)), tail_(GetSentinel()),
        offset_(Parent::GetAlignedSize()) {}

  // TODO: Don't ignore this error.
  ~Bump() { (void)Reset(); }

  Result<std::byte*> Allocate(Layout layout) noexcept {
    if (!IsValid(layout))
      return cpp::fail(Error::InvalidInput);

    std::size_t request_size = internal::AlignUp(layout.size, layout.alignment);
    DINFO("Request Size: " << request_size);

    if (request_size > GetMaxRequestSize())
      return cpp::fail(Error::SizeRequestTooLarge);

    // The loop here is a little deceiving. The intention here is not to
    // loop endlessly. Instead, the loop aids the subsequent atomic operations.
    // In practice, no more than one iteration will be needed.
    while (true) {
      std::size_t current_offset = offset_.load();
      std::size_t headroom = Parent::GetAlignedSize() - current_offset;
      if (headroom < request_size) {
        if (!Parent::kGrowWhenFull)
          return cpp::fail(Error::ReachedMemoryLimit);

        if (auto result = AllocateNewBlock(); result.has_error())
          return cpp::fail(result.error());

        // Re-enter this loop after initializing the block so that we can get
        // the latest values of |offset_| and |tail_|.
        continue;
      }

      std::size_t new_offset = current_offset + request_size;
      if (offset_.compare_exchange_weak(current_offset, new_offset))
        return reinterpret_cast<std::byte*>(tail_) + current_offset;
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
    std::lock_guard<std::mutex> guard(block_allocation_mutex);
    auto result = Parent::ReleaseAllBlocks(tail_, GetSentinel());

    tail_ = GetSentinel();
    // When |tail_| is set to the sentinel block, offset_ is placed right
    // at the boundary of the block size. This ensures that no allocation
    // request is attempted on the read-only sentinel block, and instead
    // a new block is requested from the system.
    offset_.store(Parent::GetAlignedSize());

    return result;
  }

private:
  // The sentinel block header is used for ergonomics. Given that the allocated
  // blocks are stored in a singly-linked list, there's a need for checking if
  // the first block is set, in other words, if it is not nullptr. Instead of
  // requiring a nullptr check, the first block can be set to this dummy node
  // which holds no space. Then, the logic for allocating the first "real" block
  // and all subsequent ones remain the same, no need to check for nullptr.
  // The values are not important here so the array is initialized to 0.
  static constexpr std::byte kSentinel[internal::GetBlockHeaderSize()] = {};

  static internal::BlockHeader* GetSentinel() {
    return reinterpret_cast<internal::BlockHeader*>(
        const_cast<std::byte*>(&kSentinel[0]));
  }

  // Max size allowed per request when accounting for aligned size and block
  // header.
  constexpr std::size_t GetMaxRequestSize() {
    return Parent::GetAlignedSize() - internal::GetBlockHeaderSize();
  }

  Result<void> AllocateNewBlock() {
    std::size_t current_block_count = block_count.load();
    {
      std::lock_guard<std::mutex> guard(block_allocation_mutex);

      // Check if value changed. If so, then another thread beat this one to
      // allocate new block. In that case, let's return early and this thread
      // *should* now have enough headroom in the current |tail_| block to
      // complete the allocation request.
      if (!block_count.compare_exchange_weak(current_block_count,
                                             current_block_count))
        return {};

      // Otherwise, this thread is now the leader in charge of allocating a new
      // block.
      auto block_or = Parent::AllocateNewBlock(tail_);
      if (block_or.has_error())
        return cpp::fail(Error::OutOfMemory);

      auto block = block_or.value();
      tail_ = block;
      // Always start offset at the boundary right past the BlockHeader size.
      offset_.store(internal::GetBlockHeaderSize());
      ++block_count;
    }

    return {};
  }

  // Singly-linked list containing all allocated blocks. Note, that this
  // class only tracks the *tail* of the list. When a new block is allocated,
  // it's |next| pointer is set to the current tail, before |tail_| is set
  // to the new block.
  internal::BlockHeader* tail_;

  // Offset tracking next available allocation in the current block (tail_).
  std::atomic<std::size_t> offset_;

  // Mutex used for allocating new block.
  std::mutex block_allocation_mutex;

  // Number of blocks allocated thus far.
  std::atomic<std::size_t> block_count = 0;
};

} // namespace dmt::allocator
