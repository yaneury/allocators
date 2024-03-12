#include <allocators/provider/lock_free_page.hpp>
#include <allocators/provider/static.hpp>
#include <allocators/strategy/freelist.hpp>
#include <allocators/strategy/lock_free_bump.hpp>

// Compile test that ensure all classes can be compiled with all arguments
// provided. This is necessary because template classes are not actually
// fully-defined until they are typed. Thus, certain errors scenarios
// can be hidden if not compiled with.
using LockFreePage = allocators::provider::LockFreePage<
    allocators::provider::LockFreePageParams::LimitT<100>>;

using Static = allocators::provider::Static</*Size=*/1>;
using FreeList = allocators::strategy::FreeList<
    LockFreePage,
    allocators::strategy::FreeListParams::AlignmentT<sizeof(void*)>,
    allocators::strategy::FreeListParams::SearchT<
        allocators::strategy::FreeListParams::FindBy::BestFit>>;
using LockFreeBump = allocators::strategy::LockFreeBump<LockFreePage>;
