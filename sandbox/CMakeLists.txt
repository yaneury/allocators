cmake_minimum_required(VERSION 3.15)

project(cppalloc-sandbox LANGUAGES CXX)

file(GLOB SOURCES *.cpp)
add_executable(sandbox ${SOURCES})

# Link to cppalloc library
target_link_libraries(sandbox PRIVATE cppalloc)

if(CPPALLOC_DEBUG)
  add_definitions(-DDEBUG)
endif()
