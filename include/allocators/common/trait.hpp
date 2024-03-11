#pragma once

#include <cstddef>

#include <allocators/common/error.hpp>
#include <allocators/internal/util.hpp>

#if __cplusplus >= 202002L
#include <concepts>
#endif

namespace allocators {

// A parameter used for making an allocation request.
struct Layout {
  // Number of bytes requested for allocation.
  std::size_t size;

  // Alignment for allocated bytes.
  // Value must be a power of two and greater than or equal to
  // the current running architecture's word size, i.e. `sizeof(void*)`.
  std::size_t alignment;

  constexpr explicit Layout(std::size_t size, std::size_t alignment)
      : size(size), alignment(alignment) {}
};

[[gnu::const]] inline bool IsValid(Layout layout) {
  return internal::IsValidRequest(layout.size, layout.alignment);
}

template <class T>
concept AllocatorTrait = requires(T allocator, std::size_t size, Layout layout,
                                  std::byte* bytes) {
  { allocator.Allocate(layout) } -> std::same_as<Result<std::byte*>>;
  { allocator.Allocate(size) } -> std::same_as<Result<std::byte*>>;
  { allocator.Release(bytes) } -> std::same_as<Result<void>>;
};

template <class T>
concept StrategyTrait = requires(T strategy, const T const_strategy,
                                 std::size_t size, Layout layout,
                                 std::byte* bytes) {
  { strategy.Find(layout) } -> std::same_as<Result<std::byte*>>;
  { strategy.Find(size) } -> std::same_as<Result<std::byte*>>;
  { strategy.Return(bytes) } -> std::same_as<Result<void>>;
  { strategy.Reset() } -> std::same_as<Result<void>>;

  { const_strategy.AcceptsAlignment() } -> std::same_as<bool>;
  { const_strategy.AcceptsReturn() } -> std::same_as<bool>;
};

template <class T>
concept ProviderTrait = requires(T provider, const T const_provider,
                                 std::size_t count, std::byte* bytes) {
  { provider.Provide(count) } -> std::same_as<Result<std::byte*>>;
  { provider.Return(bytes) } -> std::same_as<Result<void>>;
  { const_provider.GetBlockSize() } -> std::same_as<std::size_t>;
};

} // namespace allocators
