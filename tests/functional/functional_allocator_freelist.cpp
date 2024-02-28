#include <array>
#include <cstddef>

#include "catch2/catch_all.hpp"
#include "magic_enum.hpp"

#include "dmt/allocator/adapter.hpp"
#include "dmt/allocator/freelist.hpp"

#include "../util.hpp"
#include "dmt/allocator/fixed.hpp"

using namespace dmt::allocator;

using T = long;
static constexpr std::size_t SizeOfT = sizeof(T);
static constexpr std::size_t kChunkSize =
    SizeOfT + internal::GetBlockHeaderSize();
static constexpr std::size_t kBlockSize = 4096;
static constexpr std::size_t N = kBlockSize / kChunkSize;

template <class... Allocator> struct AllocatorPack {};

template <class... Args>
using FixedFreeList =
    FreeList<GrowT<WhenFull::ReturnNull>, SizeT<kBlockSize>, Args...>;

using FixedFreeListAllocators =
    AllocatorPack<FixedFreeList<LimitT<BlocksMust::HaveAtLeastSizeBytes>>,
                  FixedFreeList<LimitT<BlocksMust::NoMoreThanSizeBytes>>>;

TEMPLATE_LIST_TEST_CASE("Fixed FreeList allocator that can fit N objects",
                        "[allocator][FreeList][fixed]",
                        FixedFreeListAllocators) {
  TestType allocator;

  std::array<T*, N> allocs;
  for (std::size_t i = 0; i < N; ++i)
    allocs[i] = GetPtrOrFail<T>(allocator.Allocate(SizeOfT));

  if constexpr (!TestType::kMustContainSizeBytesInSpace) {
    SECTION("Can not allocate more objects when at capacity") {
      REQUIRE(allocator.Allocate(SizeOfT) == cpp::fail(Error::NoFreeBlock));
    }
  }

  SECTION("Can release all allocations") {
    for (std::size_t i = 0; i < N; ++i)
      REQUIRE(allocator.Release(ToBytePtr(allocs[i])).has_value());

    SECTION("Allowing subsequent requests of N objects") {
      for (std::size_t i = 0; i < N; ++i)
        allocs[i] = GetPtrOrFail<T>(allocator.Allocate(SizeOfT));

      for (std::size_t i = 0; i < N; ++i)
        REQUIRE(allocator.Release(ToBytePtr(allocs[i])).has_value());
    }

    SECTION("Allowing single request of ChunkSize object") {
      std::byte* chunk =
          GetValueOrFail<std::byte*>(allocator.Allocate(kChunkSize));
      REQUIRE(allocator.Release(chunk).has_value());
    }
  }
}

TEST_CASE("FreeList allocator reserves space for header",
          "[allocator][FreeList]") {
  using MockAllocator = Fixed<SizeT<kBlockSize>>;
  using AllocatorUnderTest =
      FixedFreeList<LimitT<BlocksMust::NoMoreThanSizeBytes>,
                    BlockAllocatorT<MockAllocator>>;

  MockAllocator mock;
  mock.SetDebug("mock");
  AllocatorUnderTest allocator(mock);

  auto pointer_or = allocator.Allocate(50);
  if (pointer_or.has_error())
    INFO("Pointer Error: " << (int)pointer_or.error());
  REQUIRE(pointer_or.has_value());

  std::byte* pointer_blocker_header = pointer_or.value() - GetBlockHeaderSize();
  auto buffer = reinterpret_cast<std::byte*>(mock.GetBuffer());
  // The address of buffer is not the actual addr return when calling Allocate.
  // It's higher than the address returned from Allocate. Something fishy is
  // going on here.
  REQUIRE(pointer_blocker_header == buffer + GetBlockHeaderSize());
}
