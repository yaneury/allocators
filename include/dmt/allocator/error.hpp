#pragma once

#include <result.hpp>

namespace dmt::allocator {

// Errors encountered during allocation or release requests on the allocators
// supported by this library.
enum class Error {
  // Input was malformed. The exact problem depends on the function and
  // provided input.
  InvalidInput,
  // Size requested was too large.
  SizeRequestTooLarge,
  // Allocate reached its memory capacity. This is different than |OutOfMemory|
  // where an call to request memory from the operating system fails. In this
  // case, the user-defined capacity was reached.
  ReachedMemoryLimit,
  // Couldn't locate a free block in which to place the requested memory.
  NoFreeBlock,
  // Memory allocation request failed. This means that the underlying system
  // call failed due to the system running out of memory.
  OutOfMemory,
};

template <class T> using Result = cpp::result<T, Error>;

} // namespace dmt::allocator
