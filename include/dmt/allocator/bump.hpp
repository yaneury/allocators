#pragma once

#include <array>
#include <cstdlib>
#include <dmt/allocator/parameters.hpp>
#include <dmt/allocator/trait.hpp>
#include <dmt/internal/chunk.hpp>
#include <dmt/internal/logger.hpp>
#include <dmt/internal/platform.hpp>
#include <dmt/internal/types.hpp>
#include <dmt/internal/util.hpp>
#include <mutex>
#include <template/parameters.hpp>

#ifndef DEBUG
#define DEBUG 1
#endif

#if DEBUG
#include <plog/Log.h>
#endif

namespace dmt::allocator {

template <class... Args> class Bump {
public:
  Bump() {
#if DEBUG
    PLOGD << "Instantiating allocator with following parameters: "
          << "HeaderSize: " << dmt::internal::GetChunkHeaderSize() << "\t"
          << "ObjectSize: " << ObjectSize_ << "\t"
          << "ObjectCount: " << ObjectCount_ << "\t"
          << "PerObjectAllocation: " << PerObjectAllocation << "\t"
          << "RequestSize: " << RequestSize_ << "\t"
          << "AlignedSize: " << AlignedSize_;
#endif
  }

  ~Bump() { Reset(); }

  internal::Byte* AllocateUnaligned(std::size_t size) {
    return Allocate(Layout{.size = size, .alignment = sizeof(void*)});
  }

  internal::Byte* Allocate(Layout layout) noexcept {
    // This class uses a very coarse-grained mutex for allocation.
    std::lock_guard<std::mutex> lock(chunks_mutex_);
    assert(layout.alignment >= sizeof(void*));
    std::size_t request_size = internal::AlignUp(
        layout.size + dmt::internal::GetChunkHeaderSize(), Alignment_);

#if DEBUG
    PLOGD << "[Allocate] Received layout(size=" << layout.size
          << ", alignment=" << layout.alignment << ").";
    PLOGD << "[Allocate] Request size: " << request_size;
#endif

    if (request_size > AlignedSize_)
      return nullptr;

    if (!chunks_) {
      if (chunks_ = AllocateNewChunk(); !chunks_)
        return nullptr;

      // Set current chunk to header
      current_ = chunks_;
    }

    std::size_t remaining_size = AlignedSize_ - offset_;
#if DEBUG
    PLOGD << "[Allocate] Offset: " << offset_;
    PLOGD << "[Allocator] Remaining Size: " << remaining_size;
#endif

    if (request_size > remaining_size) {
      if (!GrowWhenFull_)
        return nullptr;

      auto* chunk = AllocateNewChunk();
      if (!chunk)
        return nullptr;

      current_->next = chunk;
      current_ = chunk;
      offset_ = 0;
    }

    internal::Byte* base = dmt::internal::GetChunk(current_);
    internal::Byte* result = base + offset_;
    offset_ += request_size;

    return result;
  }

  void Release(internal::Byte*) {
    // The bump allocator does not support per-object deallocation.
  }

  void Reset() {
    std::lock_guard<std::mutex> lock(chunks_mutex_);
    offset_ = 0;
    if (chunks_)
      ReleaseChunks(chunks_);
    chunks_ = nullptr;
  }

  static constexpr std::size_t Alignment_ =
      std::max({sizeof(void*), ntp::optional<AlignmentT<0>, Args...>::value});

  static_assert(internal::IsPowerOfTwo(Alignment_),
                "Alignment must be a power of 2.");

  static constexpr bool GrowWhenFull_ =
      ntp::optional<GrowT<WhenFull::GrowStorage>, Args...>::value ==
      WhenFull::GrowStorage;

  static constexpr std::size_t ObjectSize_ =
      ntp::optional<ObjectSizeT<0>, Args...>::value;

  static constexpr std::size_t ObjectCount_ =
      ntp::optional<ObjectCountT<0>, Args...>::value;

  static constexpr bool PerObjectAllocation =
      ObjectSize_ > 0 && ObjectCount_ > 0;

  // If allocator is using PerObjectAllocation, then set size to be amount
  // requested by user: ObjectSize * ObjectCount; otherwise, go with
  // RequestSize. To ensure that necessary objects fit, we also multiple
  // ObjectCount* with header size.
  static constexpr std::size_t RequestSize_ =
      PerObjectAllocation
          ? ObjectCount_ * (ObjectSize_ + dmt::internal::GetChunkHeaderSize())
          : ntp::optional<SizeT<kDefaultSize>, Args...>::value;

  static constexpr std::size_t AlignedSize_ = internal::AlignUp(
      RequestSize_ + internal::GetChunkHeaderSize(), Alignment_);

private:
  static internal::Allocation CreateAllocation(internal::Byte* base) {
    std::size_t size = IsPageMultiple() ? AlignedSize_ / internal::GetPageSize()
                                        : AlignedSize_;
    return internal::Allocation{.base = static_cast<internal::Byte*>(base),
                                .size = size};
  }

  static bool IsPageMultiple() {
    static const auto page_size = internal::GetPageSize();
    return AlignedSize_ >= page_size && AlignedSize_ % page_size == 0;
  }

  static dmt::internal::ChunkHeader* AllocateNewChunk() {
    auto allocation =
        IsPageMultiple()
            ? internal::AllocatePages(AlignedSize_ / internal::GetPageSize())
            : internal::AllocateBytes(AlignedSize_, Alignment_);

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
};

} // namespace dmt::allocator