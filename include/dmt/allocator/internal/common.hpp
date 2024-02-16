#pragma once

#include <cstdint>

namespace dmt::allocator::internal {

// 4KB Page Size (1 << 12)
static constexpr std::size_t kSmallPageShift = 12;

// 4MB Page Size (1 << 22)
static constexpr std::size_t kLargePageShift = 22;

// 4GB Page Size (1 << 32)
static constexpr std::size_t kHugePageShift = 32;

}