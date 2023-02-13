#pragma once

#include <array>
#include <cstdlib>
#include <libdmt/internal/platform.hpp>
#include <libdmt/internal/types.hpp>
#include <libdmt/internal/util.hpp>

namespace dmt::allocator {

// Default size for the storage backed by the Bump allocator.
static constexpr std::size_t kDefaultSize = 4096;

struct SizeId {};

template <std::size_t Size>
struct SizeT : std::integral_constant<std::size_t, Size> {
  using Id_ = SizeId;
};

struct AlignmentId {};

template <std::size_t Alignment>
struct AlignmentT : std::integral_constant<std::size_t, Alignment> {
  using Id_ = AlignmentId;
};

struct WhenFullId {};

enum WhenFull { ReturnNull = 0, GrowStorage = 1 };

template <WhenFull WF> struct GrowT : std::integral_constant<WhenFull, WF> {
  using Id_ = WhenFullId;
};

// TODO: Arena allocations when at capacity and using heap
template <class T, typename... Args> class Bump {
public:
  // Require alias for std::allocator_traits to infer other types, e.g.
  // using pointer = value_type*.
  using value_type = T;

  explicit Bump(){};

  ~Bump() { Reset(); }

  template <class U> constexpr Bump(const Bump<U>&) noexcept {}

  T* allocate(std::size_t n) noexcept {
    if (n > AlignedSize_)
      return nullptr;

    if (!chunks_)
      if (chunks_ = AllocateNewChunk(); !chunks_)
        return nullptr;

    size_t request_size = internal::AlignUp(n, Alignment_);
    size_t remaining_size =
        AlignedSize_ - offset_ - ChunkHeader::GetChunkHeaderSize();

    if (request_size > remaining_size) {
      // TODO: Get new chunk
      return nullptr;
    }

    Byte* base = ChunkHeader::GetChunk(chunks_);
    Byte* result = base + offset_;
    offset_ += request_size;

    return reinterpret_cast<T*>(result);
  }

  void deallocate(T*, std::size_t) noexcept {
    // The bump allocator does not support per-object deallocation.
  }

  void Reset() {
    offset_ = 0;
    if (chunks_)
      ReleaseChunk(chunks_);
    chunks_ = nullptr;
  }

private:
  using Byte = uint8_t;

  struct ChunkHeader {
    ChunkHeader* next = nullptr;

    static constexpr std::size_t GetChunkHeaderSize() {
      return sizeof(ChunkHeader);
    }

    static Byte* GetChunk(ChunkHeader* header) {
      return reinterpret_cast<Byte*>(header) + sizeof(header->next);
    }
  };

  // There are several factors used to determine the alignment for the
  // allocator. First, users can specify their own alignment if desired using
  // |AlignmentT<>|. Otherwise, we use the alignment as determined by the C++
  // compiler. There's a floor in the size of the alignment to be equal to or
  // greater than |sizeof(void*)| for compatibility with std::aligned_alloc.
  static constexpr std::size_t Alignment_ =
      std::max({std::alignment_of_v<T>, sizeof(void*),
                internal::GetValueT<AlignmentT<0>, Args...>::value});

  static_assert(internal::IsPowerOfTwo(Alignment_),
                "Alignment must be a power of 2.");

  static constexpr std::size_t RequestSize_ =
      internal::GetValueT<SizeT<kDefaultSize>, Args...>::value;

  static constexpr std::size_t AlignedSize_ = internal::AlignUp(
      RequestSize_ + ChunkHeader::GetChunkHeaderSize(), Alignment_);

  static constexpr bool GrowWhenFull_ =
      internal::GetValueT<GrowT<WhenFull::GrowStorage>, Args...>::value ==
      WhenFull::GrowStorage;

  static internal::Allocation CreateAllocation(Byte* base) {
    std::size_t size = IsPageMultiple() ? AlignedSize_ / internal::GetPageSize()
                                        : AlignedSize_;
    return internal::Allocation{.base = static_cast<internal::Byte*>(base),
                                .size = size};
  }

  static bool IsPageMultiple() {
    static const auto page_size = internal::GetPageSize();
    return AlignedSize_ > page_size && AlignedSize_ % page_size == 0;
  }

  static ChunkHeader* AllocateNewChunk() {
    auto allocation =
        IsPageMultiple()
            ? internal::AllocatePages(AlignedSize_ / internal::GetPageSize())
            : internal::AllocateBytes(AlignedSize_, Alignment_);

    if (!allocation.has_value())
      return nullptr;

    Byte* base = allocation.value().base;
    memset(static_cast<void*>(base), 0, allocation.value().size);
    ChunkHeader* header =
        reinterpret_cast<ChunkHeader*>(allocation.value().base);
    header->next = nullptr;
    return header;
  }

  static void ReleaseChunk(ChunkHeader* chunk) {
    auto allocation = CreateAllocation(reinterpret_cast<Byte*>(chunk));
    if (IsPageMultiple()) {
      internal::ReleasePages(allocation);
    } else {
      internal::ReleaseBytes(allocation);
    }
  }

  size_t offset_ = 0;
  ChunkHeader* chunks_ = nullptr;
};

template <class T, class U> bool operator==(const Bump<T>&, const Bump<U>&) {
  return true;
}

template <class T, class U> bool operator!=(const Bump<T>&, const Bump<U>&) {
  return false;
}

} // namespace dmt::allocator