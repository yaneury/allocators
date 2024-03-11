project(tests LANGUAGES CXX)

add_executable(
  ${PROJECT_NAME}
  test.cpp
  performance/all_performance_test.cpp
  concurrency/bump_concurrency_test.cpp
  concurrency/page_concurrency_test.cpp
  functional/freelist_functional_test.cpp
  functional/all_functional_test.cpp
  functional/internal_functional_test.cpp
  functional/page_functional_test.cpp)

# Link to allocators library
target_link_libraries(${PROJECT_NAME} PRIVATE allocators)

# Fetch and install Catch2
include(FetchContent)

FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG v3.4.0)

FetchContent_MakeAvailable(Catch2)

target_link_libraries(${PROJECT_NAME} PRIVATE Catch2::Catch2WithMain)

include(CTest)
include(Catch)

# Fetch and install atomic_queue
FetchContent_Declare(
  atomic_queue
  GIT_REPOSITORY https://github.com/max0x7ba/atomic_queue.git
  GIT_TAG v1.6.3)
FetchContent_MakeAvailable(atomic_queue)

target_link_libraries(${PROJECT_NAME} PRIVATE max0x7ba::atomic_queue)

enable_testing()

add_test(NAME ${PROJECT_NAME}
         COMMAND $<TARGET_FILE:${PROJECT_NAME}> --skip-benchmarks
                 --allow-running-no-tests)
