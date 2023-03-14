#pragma once

#include <mutex>

#include <dmt/allocator/block.hpp>
#include <dmt/allocator/trait.hpp>
#include <dmt/internal/block.hpp>
#include <dmt/internal/util.hpp>
#include <template/parameters.hpp>

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
  Bump() { DINFO("kAlignedSize: " << Parent::kAlignedSize_); }

  ~Bump() { Reset(); }

  std::byte* AllocateUnaligned(std::size_t size) noexcept {
    return Allocate(
        Layout{.size = size, .alignment = internal::kMinimumAlignment});
  }

  std::byte* Allocate(Layout layout) noexcept {
    if (!IsValid(layout))
      return nullptr;

    std::size_t request_size = internal::AlignUp(layout.size, layout.alignment);
    DINFO("Request Size: " << request_size);

    if (request_size > kMaxRequestSize_)
      return nullptr;

    // This class uses a very coarse-grained mutex for allocation.
    std::lock_guard<std::mutex> lock(blocks_mutex_);

    if (!blocks_) {
      if (blocks_ = Parent::AllocateNewBlock(); !blocks_)
        return nullptr;

      // Set current block to header.
      current_ = blocks_;
      offset_ = 0;
    }

    std::size_t remaining_size =
        Parent::kAlignedSize_ - internal::GetBlockHeaderSize() - offset_;
    DINFO("Remaining Size: " << remaining_size);

    if (request_size > remaining_size) {
      if (!Parent::kGrowWhenFull)
        return nullptr;

      auto* block = Parent::AllocateNewBlock();
      if (!block)
        return nullptr;

      current_->next = block;
      current_ = block;
      offset_ = 0;
    }

    std::byte* base = internal::GetBlock(current_);
    std::byte* result = base + offset_;
    offset_ += request_size;

    return result;
  }

  void Release(std::byte*) {
    // The bump allocator does not support per-object deallocation.
  }

  void Reset() {
    std::lock_guard<std::mutex> lock(blocks_mutex_);
    offset_ = 0;
    if (blocks_)
      Parent::ReleaseBlocks(blocks_);
    blocks_ = nullptr;
  }

private:
  // We have to explicitly provide the parent class in contexts with a
  // nondepedent name. For more information, see:
  // https://stackoverflow.com/questions/75595977/access-protected-members-of-base-class-when-using-template-parameter.
  using Parent = Block<Args...>;

  // Max size allowed per request when accounting for aligned size and block
  // header.
  static constexpr std::size_t kMaxRequestSize_ =
      Parent::kAlignedSize_ - internal::GetBlockHeaderSize();

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