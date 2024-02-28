cmake_minimum_required(VERSION 3.15)

project(dmt-tests LANGUAGES CXX)

add_executable(
  ${PROJECT_NAME}
  test.cpp
  performance/performance_allocator_all.cpp
  concurrency/concurrency_allocator_bump.cpp
  concurrency/concurrency_allocator_page.cpp
  functional/functional_allocator_freelist.cpp
  functional/functional_allocator_all.cpp
  functional/functional_allocator_internal.cpp
  functional/functional_allocator_fixed.cpp
  functional/functional_allocator_page.cpp)

# Link to dmt library
target_link_libraries(${PROJECT_NAME} PRIVATE dmt)

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
catch_discover_tests(${PROJECT_NAME})

# # Fetch and install Boost
set(BOOST_INCLUDE_LIBRARIES lockfree)
set(BOOST_ENABLE_CMAKE ON)
FetchContent_Declare(
  Boost
  GIT_REPOSITORY https://github.com/boostorg/boost.git
  GIT_TAG boost-1.84.0)
FetchContent_MakeAvailable(Boost)

target_link_libraries(${PROJECT_NAME} PRIVATE Boost::lockfree)

enable_testing()

add_test(NAME ${PROJECT_NAME} COMMAND $<TARGET_FILE:${PROJECT_NAME}>
                                      --skip-benchmarks)

if(DMT_DEBUG)
  add_definitions(-DDEBUG)
endif()
