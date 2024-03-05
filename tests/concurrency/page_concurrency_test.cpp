#include "catch2/catch_all.hpp"

#include <mutex>
#include <thread>
#include <vector>

#include "atomic_queue/atomic_queue.h"

#include "dmt/allocator/page.hpp"

using namespace dmt::allocator;

using AllocatorUnderTest = Page<>;

TEST_CASE("Page allocator works in multi-threaded contexts",
          "[concurrency][allocator][Page]") {
  static constexpr std::size_t kMaxPages = 1;
  static constexpr std::size_t kMaximumOps = 1;
  static constexpr std::size_t kNumThreads = 2;
  static_assert(kNumThreads % 2 == 0, "number of threads must even");

  AllocatorUnderTest allocator;
  atomic_queue::AtomicQueue<std::byte*, kMaxPages> allocations;
  // Mutex used for calling Catch2's APIs
  std::mutex catch_mutex;

  auto allocate = [&]() {
    for (std::size_t i = 0; i < kMaximumOps; ++i) {
      auto p_or = allocator.Allocate(1);
      if (p_or.has_error()) {
        {
          std::scoped_lock lock(catch_mutex);
          INFO("[" << std::this_thread::get_id()
                   << "] Allocation failed: " << ToString(p_or.error()));
          FAIL();
        }
      }

      while (!allocations.try_push(p_or.value()))
        ;
    }
  };

  auto release = [&]() {
    for (std::size_t i = 0; i < kMaximumOps; ++i) {
      std::byte* p = nullptr;
      while (!allocations.try_pop(p))
        ;

      auto result = allocator.Release(p);
      if (result.has_error()) {
        std::scoped_lock<std::mutex> lock(catch_mutex);
        INFO("[" << std::this_thread::get_id()
                 << "] Release failed: " << ToString(result.error()));
        FAIL();
      }
    }
  };

  std::vector<std::thread> threads;
  for (auto i = 0ul; i < kNumThreads; ++i) {
    if (i % 2)
      threads.emplace_back(allocate);
    else
      threads.emplace_back(release);
  }

  for (auto& th : threads)
    th.join();

  REQUIRE(allocations.was_empty());
}
