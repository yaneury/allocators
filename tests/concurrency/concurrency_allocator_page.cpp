#include "catch2/catch_all.hpp"

#include <atomic>
#include <thread>
#include <vector>

#include "dmt/allocator/page.hpp"

#include "thread_safe_container.hpp"

using namespace dmt::allocator;

TEST_CASE("Page allocator", "[concurrency][allocator][Page]") {
  static constexpr std::size_t kNumThreads = 4;

  using AllocatorUnderTest = Page<>;
  AllocatorUnderTest allocator;

  std::atomic<std::size_t> requests_made = 0;
  dmt::testing::ThreadSafeQueue<std::byte*> allocations;
  std::mutex message_lock;

  auto chaos_allocate = [&](std::size_t id) {
    while (true) {
      auto old_allocation = requests_made.load();
      if (old_allocation == AllocatorUnderTest::kCount)
        return;

      if (requests_made.compare_exchange_weak(old_allocation,
                                              old_allocation + 1)) {
        auto p_or = allocator.Allocate(1);
        if (p_or.has_error()) {
          {
            std::lock_guard<std::mutex> guard(message_lock);
            INFO("[" << id
                     << "] Allocation failed: " << ToString(p_or.error()));
            FAIL();
          }
        }

        allocations.Push(p_or.value());
      }
    }
  };

  std::vector<std::thread> threads;
  threads.reserve(kNumThreads);
  for (auto i = 0ul; i < kNumThreads; ++i) {
    threads.emplace_back(chaos_allocate, i);
  }

  for (auto& th : threads)
    th.join();

  threads.clear();

  auto chaos_release = [&](std::size_t id) {
    while (true) {
      auto p_or = allocations.Pop();
      if (!p_or.has_value())
        return;

      auto result = allocator.Release(p_or.value());
      if (result.has_error()) {
        {
          std::lock_guard<std::mutex> guard(message_lock);
          INFO("[" << id << "] Release failed: " << ToString(result.error()));
          FAIL();
        }
      }
    }
  };

  threads.reserve(kNumThreads);
  for (auto i = 0ul; i < kNumThreads; ++i) {
    threads.emplace_back(chaos_release, i);
  }

  for (auto& th : threads)
    th.join();

  threads.clear();
}
