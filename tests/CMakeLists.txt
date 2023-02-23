cmake_minimum_required(VERSION 3.15)

project(dmt-tests LANGUAGES CXX)

add_executable(${PROJECT_NAME} bump.cpp internal.cpp test.cpp)

# Link to dmt library
target_link_libraries(${PROJECT_NAME} PRIVATE dmt)

# Fetch and install Catch2
include(FetchContent)

FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG v3.0.1)

FetchContent_MakeAvailable(Catch2)

target_link_libraries(${PROJECT_NAME} PRIVATE Catch2::Catch2WithMain)

include(CTest)
include(Catch)
catch_discover_tests(${PROJECT_NAME})
enable_testing()

if(DMT_DEBUG)
  add_definitions(-DDEBUG)
endif()
