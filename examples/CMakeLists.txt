cmake_minimum_required(VERSION 3.15)

project(dmt-examples LANGUAGES CXX)

add_executable(allocator allocator.cpp)

# Link to dmt library
target_link_libraries(allocator PRIVATE dmt)

add_executable(handle handle.cpp)

# Link to dmt library
target_link_libraries(handle PRIVATE dmt)

if(DMT_DEBUG)
  add_definitions(-DDEBUG)
endif()
