# Allocators 
A header-only C++ library offering several specialized memory allocators configurable at compile-time.

[![Ubuntu][linux-badge]][linux-status] [![macOS][mac-badge]][mac-status]

## Motivation
The goal of this project is to provide developers with specialized memory allocators that accommodate various
use cases. The allocators are highly configurable, allowing for easy tuning using the C++ language interface
(not just with `#define` macros). They're composable, with multiple allocators being able to stack on top of
each other. For standard library support, all allocators contain adapter classes that implement [`std::allocator_traits`][allocator-traits].

## Allocators
Allocators are split into two categories: object and block. Object allocators are fine-grained classes used to
instantiate objects. This is the common use case, where, for example, a new object of type `T` is created on the
heap. In other words, this is what most usages of allocators are for. The second type, block allocators, returns
large blocks of memory, within which multiple objects can be allocated. All object allocators used a block
allocator to fetch memory from the OS, creating an extra layer of abstraction between object and block allocation.

The API for both allocator categories are similar, but not the same. The API of the object allocators is similar
to the standard `malloc` and `new` operator interface that developers are accustomed to. However, they're a bit more
robust in that have an opinionated error handling at the call site, as opposed to relying on `errno` or exceptions.
Also, alignment is an explicit parameter for allocations. Though not explicitly defined in the code, the interface looks
like the following:

```cpp
struct Layout {
    std::size_t size;
    std::size_t alignment;
};

class ObjectAllocator {
public:
    Result<std::byte*> Allocate(Layout layout);
    
    Result<std::byte*> Allocate(std::size_t size);
    
    Result<void> Release(std::byte* ptr);
};
```

The BlockAllocator interface differs in the `Allocate` method where instead of using the size of bytes
to request, a count of blocks is requested.

```cpp
class BlockAllocator {
    public:  
        Result<std::byte*> Allocate(std::size_t count);
        
        Result<void> Release(std::byte* ptr);
        
        constexpr std::size_t GetBlockSize() const;
};
```

The next section shows the allocators that are either currently available or on the roadmap.

### Object Allocators
* **Bump**: Uses an offset within blocks of memories to fulfill requests.
* **Static**: Fulfills requests over a statically-defined block.
* **Freelist**: List-based allocator supporting different search policies.
* **Slab**: Extension of Freelist allocator that maintains separate blocks for different object sizes.
* **Buddy**: Tree-based allocator that separates blocks into smaller chunks that are powers of 2.

### Block Allocators
* **Page**: Allocator that fetches page-sized blocks. The size of the page is determined by the platform, typically 4KB.
* **HugePage**: Allocator that fetches huge pages, if the system supports it.

## Examples
TODO

## Compatability
Currently, the library only works for `c++20` and above. However, support for `c++11` will be added in the [future][cpp11-issue]. Generally, only `clang` and `gcc` are regularly tested on Linux and macOS. Windows
support will also be [added][windows-issue].

## License
**allocators** is licensed under the [MIT License][mit-license].

[linux-badge]: https://github.com/yaneury/allocators/actions/workflows/linux.yml/badge.svg?branch=main
[linux-status]: https://github.com/yaneury/allocators/actions/workflows/linux.yml
[mac-badge]: https://github.com/yaneury/allocators/actions/workflows/mac.yml/badge.svg?branch=main
[mac-status]: https://github.com/yaneury/allocators/actions/workflows/mac.yml
[allocator-traits]: https://en.cppreference.com/w/cpp/memory/allocator_traits
[cpp11-issue]: https://github.com/yaneury/allocators/issues/29
[windows-issue]: https://github.com/yaneury/allocators/issues/30
[mit-license]: http://opensource.org/licenses/MIT

