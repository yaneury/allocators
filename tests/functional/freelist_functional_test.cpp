#include <array>
#include <cstddef>

#include "catch2/catch_all.hpp"
#include "magic_enum.hpp"

#include <allocators/adapter/adapter.hpp>
#include <allocators/strategy/freelist.hpp>

#include "../util.hpp"
#include <allocators/provider/lock_free_page.hpp>
#include <allocators/provider/static.hpp>

using namespace allocators;

using T = long;
static constexpr std::size_t SizeOfT = sizeof(T);
static constexpr std::size_t kChunkSize =
    SizeOfT + internal::GetBlockHeaderSize();
static constexpr std::size_t kBlockSize = 4096;
static constexpr std::size_t N = kBlockSize / kChunkSize;

template <class... Allocator> struct AllocatorPack {};

template <class... Args>
using FixedFreeList = strategy::FreeList<provider::LockFreePage<>, Args...>;

using FixedFreeListAllocators = AllocatorPack<FixedFreeList<>>;

TEMPLATE_LIST_TEST_CASE("Fixed FreeList allocator that can fit N objects",
                        "[allocator][FreeList][fixed]",
                        FixedFreeListAllocators) {
  // TODO: Enable once fixed.
  SKIP();
  provider::LockFreePage<> provider;
  TestType allocator(provider);

  std::array<T*, N> allocs;
  for (std::size_t i = 0; i < N; ++i)
    allocs[i] = GetPtrOrFail<T>(allocator.Find(SizeOfT));

  SECTION("Can release all allocations") {
    for (std::size_t i = 0; i < N; ++i)
      REQUIRE(allocator.Return(ToBytePtr(allocs[i])).has_value());

    SECTION("Allowing subsequent requests of N objects") {
      for (std::size_t i = 0; i < N; ++i)
        allocs[i] = GetPtrOrFail<T>(allocator.Find(SizeOfT));

      for (std::size_t i = 0; i < N; ++i)
        REQUIRE(allocator.Return(ToBytePtr(allocs[i])).has_value());
    }

    SECTION("Allowing single request of ChunkSize object") {
      std::byte* chunk = GetValueOrFail<std::byte*>(allocator.Find(kChunkSize));
      REQUIRE(allocator.Return(chunk).has_value());
    }
  }
}
