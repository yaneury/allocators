#pragma once

#include <array>
#include <cstdlib>
#include <dmt/allocator/parameters.hpp>
#include <dmt/allocator/trait.hpp>
#include <dmt/internal/chunk.hpp>
#include <dmt/internal/log.hpp>
#include <dmt/internal/platform.hpp>
#include <dmt/internal/util.hpp>
#include <mutex>
#include <template/parameters.hpp>

namespace dmt::allocator {

// A simple Bump allocator. This allocator creates a big block of bytes on
// first allocation, hereafter "chunk", that fits a large number of objects.
// Each allocation moves a pointer upward, tracking the location of the most
// recent allocation. When an allocation request is made that is greater than
// the size remaining in the current chunk, this allocator may optionally
// request for more memory, creating a linked list of chunks. Per-object
// allocation, i.e. the standard `free` call, is not supported. Instead, this
// allocator only supports freeing an entire chunk(s).
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
  // Alignment used for the chunks requested. N.b. this is *not* the alignment
  // for individual allocation requests, of which may have different alignment
  // requirements.
  //
  // This field is optional. If not provided, will default to |sizeof(void*)|.
  // If provided, it must greater than |sizeof(void*)| and be a power of two.
  static constexpr std::size_t kAlignment =
      std::max({sizeof(void*), ntp::optional<AlignmentT<0>, Args...>::value});

  // Size of the chunks. This allocator doesn't support variable-sized chunks.
  // All chunks allocated are of the same size. N.b. that the size here will
  // *not* be the size of memory ultimately requested for chunks. This is so
  // because supplemental memory is needed for chunk headers and to ensure
  // alignment as specified with |kAlignment|.
  //
  // This field is optional. If not provided, will default to the common page
  // size, 4096.
  static constexpr std::size_t kSize =
      ntp::optional<SizeT<4096>, Args...>::value;

  // Policy employed when chunk has no more space for pending request.
  // If |GrowStorage| is provided, then a new chunk will be requested;
  // if |ReturnNull| is provided, then nullptr is returned on the allocation
  // request. This does not mean that it's impossible to request more memory
  // though. It only means that the chunk has no more space for the requested
  // size. If a smaller size request comes along, it may be possible that the
  // chunk has sufficient storage for it.
  static constexpr bool kGrowWhenFull =
      ntp::optional<GrowT<WhenFull::GrowStorage>, Args...>::value ==
      WhenFull::GrowStorage;

  Bump() = default;

  ~Bump() { Reset(); }

  std::byte* AllocateUnaligned(std::size_t size) {
    return Allocate(Layout{.size = size, .alignment = sizeof(void*)});
  }

  std::byte* Allocate(Layout layout) noexcept {
    // This class uses a very coarse-grained mutex for allocation.
    std::lock_guard<std::mutex> lock(chunks_mutex_);
    assert(layout.alignment >= sizeof(void*));
    std::size_t request_size = internal::AlignUp(
        layout.size + dmt::internal::GetChunkHeaderSize(), kAlignment);

    DINFO("[Allocate] Received layout(size=" << layout.size << ", alignment="
                                             << layout.alignment << ").");
    DINFO("[Allocate] Request size: " << request_size);

    if (request_size > kAlignedSize_)
      return nullptr;

    if (!chunks_) {
      if (chunks_ = AllocateNewChunk(); !chunks_)
        return nullptr;

      // Set current chunk to header
      current_ = chunks_;
    }

    std::size_t remaining_size = kAlignedSize_ - offset_;
    DINFO("[Allocate] Offset: " << offset_);
    DINFO("[Allocator] Remaining Size: " << remaining_size);

    if (request_size > remaining_size) {
      if (!kGrowWhenFull)
        return nullptr;

      auto* chunk = AllocateNewChunk();
      if (!chunk)
        return nullptr;

      current_->next = chunk;
      current_ = chunk;
      offset_ = 0;
    }

    std::byte* base = dmt::internal::GetChunk(current_);
    std::byte* result = base + offset_;
    offset_ += request_size;

    return result;
  }

  void Release(std::byte*) {
    // The bump allocator does not support per-object deallocation.
  }

  void Reset() {
    std::lock_guard<std::mutex> lock(chunks_mutex_);
    offset_ = 0;
    if (chunks_)
      ReleaseChunks(chunks_);
    chunks_ = nullptr;
  }

private:
  // Ultimate size of the chunks after accounting for header and alignment.
  static constexpr std::size_t kAlignedSize_ =
      internal::AlignUp(kSize + internal::GetChunkHeaderSize(), kAlignment);

  static internal::Allocation CreateAllocation(std::byte* base) {
    std::size_t size = IsPageMultiple()
                           ? kAlignedSize_ / internal::GetPageSize()
                           : kAlignedSize_;
    return internal::Allocation{.base = static_cast<std::byte*>(base),
                                .size = size};
  }

  [[gnu::const]] static bool IsPageMultiple() {
    static const auto page_size = internal::GetPageSize();
    return kAlignedSize_ >= page_size && kAlignedSize_ % page_size == 0;
  }

  static dmt::internal::ChunkHeader* AllocateNewChunk() {
    auto allocation =
        IsPageMultiple()
            ? internal::AllocatePages(kAlignedSize_ / internal::GetPageSize())
            : internal::AllocateBytes(kAlignedSize_, kAlignment);

    if (!allocation.has_value())
      return nullptr;

    return dmt::internal::CreateChunkHeaderFromAllocation(allocation.value());
  }

  static void ReleaseChunks(dmt::internal::ChunkHeader* chunk) {
    auto release =
        IsPageMultiple() ? internal::ReleasePages : internal::ReleaseBytes;
    dmt::internal::ReleaseChunks(chunk, std::move(release));
  }

  size_t offset_ = 0;
  dmt::internal::ChunkHeader* chunks_ = nullptr;
  dmt::internal::ChunkHeader* current_ = nullptr;

  std::mutex chunks_mutex_;

  // Various assertions hidden from user API but added here to ensure invariants
  // are met at compile time.
  static_assert(internal::IsPowerOfTwo(kAlignment),
                "kAlignment must be a power of 2.");
};

} // namespace dmt::allocator