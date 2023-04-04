#pragma once

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

  Bump() : Parent(Allocator()) {}

  Bump(Allocator& allocator) : Parent(allocator) {}

  Bump(Allocator&& allocator) : Parent(std::move(allocator)) {}

  // TODO: Don't ignore this error.
  ~Bump() { (void)Reset(); }

  Result<std::byte*> Allocate(Layout layout) noexcept {
    if (!IsValid(layout))
      return cpp::fail(Error::InvalidInput);

    std::size_t request_size = internal::AlignUp(layout.size, layout.alignment);
    DINFO("Request Size: " << request_size);

    if (request_size > GetMaxRequestSize())
      return cpp::fail(Error::SizeRequestTooLarge);

    if (auto init = InitBlockIfUnset(); init.has_error())
      return cpp::fail(init.error());

    std::size_t remaining_size =
        Parent::GetAlignedSize() - internal::GetBlockHeaderSize() - offset_;
    DINFO("Remaining Size: " << remaining_size);

    if (request_size > remaining_size) {
      if (!Parent::kGrowWhenFull)
        return cpp::fail(Error::ReachedMemoryLimit);

      auto block_or = Parent::AllocateNewBlock();
      if (block_or.has_error())
        return cpp::fail(block_or.error());

      auto block = block_or.value();
      current_->next = block;
      current_ = block;
      offset_ = 0;
    }

    std::byte* base = internal::GetBlock(current_);
    std::byte* result = base + offset_;
    offset_ += request_size;

    return result;
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

  // TODO: Make this thread safe.
  Result<void> InitBlockIfUnset() {
    // This class uses a very coarse-grained mutex for allocation.
    std::lock_guard<std::mutex> lock(blocks_mutex_);

    if (blocks_)
      return {};

    auto new_block = Parent::AllocateNewBlock();
    if (new_block.has_error())
      return cpp::fail(Error::OutOfMemory);

    // Set current block to header.
    current_ = blocks_ = new_block.value();
    offset_ = 0;
    return {};
  }

  Allocator allocator_;

  // List of all allocated blocks.
  internal::BlockHeader* blocks_ = nullptr;

  // Current block in used.
  internal::BlockHeader* current_ = nullptr;

  // Offset for current block. This is reset everytime a new block is allocated.
  // Offset starts past block header size.
  size_t offset_ = internal::GetBlockHeaderSize();

  // Coarse-grained mutex.
  // TODO: Look into fine-grained alternatives.
  // One option is to use atomic instructions, e.g. __sync_fetch_and_add.
  std::mutex blocks_mutex_;
};

} // namespace dmt::allocator
